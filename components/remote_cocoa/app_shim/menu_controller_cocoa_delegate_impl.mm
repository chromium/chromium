// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/menu_controller_cocoa_delegate_impl.h"

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/logging.h"
#import "base/message_loop/message_pump_apple.h"
#import "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/platform_font_mac.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

constexpr CGFloat kIPHDotSize = 6;

NSImage* NewTagImage(const remote_cocoa::mojom::MenuControllerParams& params) {
  // 1. Make the attributed string.

  NSString* badge_text = l10n_util::GetNSString(IDS_NEW_BADGE);

  NSColor* badge_text_color =
      skia::SkColorToSRGBNSColor(params.badge_text_color);

  NSDictionary* badge_attrs = @{
    NSFontAttributeName :
        base::apple::CFToNSPtrCast(params.badge_font.GetCTFont()),
    NSForegroundColorAttributeName : badge_text_color,
  };

  NSMutableAttributedString* badge_attr_string =
      [[NSMutableAttributedString alloc] initWithString:badge_text
                                             attributes:badge_attrs];

  // 2. Calculate the size required.

  NSSize text_size = [badge_attr_string size];
  NSSize canvas_size =
      NSMakeSize(trunc(text_size.width) + 2 * params.badge_internal_padding +
                     2 * params.badge_horizontal_margin,
                 fmax(trunc(text_size.height), params.badge_min_height));

  // 3. Craft the image.

  return [NSImage
       imageWithSize:canvas_size
             flipped:NO
      drawingHandler:^(NSRect dest_rect) {
        NSRect badge_frame =
            NSInsetRect(dest_rect, params.badge_horizontal_margin, 0);
        NSBezierPath* rounded_badge_rect =
            [NSBezierPath bezierPathWithRoundedRect:badge_frame
                                            xRadius:params.badge_radius
                                            yRadius:params.badge_radius];
        NSColor* badge_color = skia::SkColorToSRGBNSColor(params.badge_color);

        [badge_color set];
        [rounded_badge_rect fill];

        // Place the text rect at the center of the badge frame.
        NSPoint badge_text_location =
            NSMakePoint(NSMinX(badge_frame) +
                            (badge_frame.size.width - text_size.width) / 2.0,
                        NSMinY(badge_frame) +
                            (badge_frame.size.height - text_size.height) / 2.0);
        [badge_attr_string drawAtPoint:badge_text_location];

        return YES;
      }];
}

NSImage* IPHDotImage(const remote_cocoa::mojom::MenuControllerParams& params) {
  // Embed horizontal centering space as NSMenuItem will otherwise left-align
  // it.
  return [NSImage
       imageWithSize:NSMakeSize(2 * kIPHDotSize, kIPHDotSize)
             flipped:NO
      drawingHandler:^(NSRect dest_rect) {
        NSBezierPath* dot_path = [NSBezierPath
            bezierPathWithOvalInRect:NSMakeRect(kIPHDotSize / 2, 0, kIPHDotSize,
                                                kIPHDotSize)];
        NSColor* dot_color = skia::SkColorToSRGBNSColor(params.iph_dot_color);
        [dot_color set];
        [dot_path fill];

        return YES;
      }];
}

}  // namespace

// --- Private API begin ---

// In macOS 13 and earlier, the internals of menus are handled by HI Toolbox,
// and the bridge to that code is NSCarbonMenuImpl. While in reality this is a
// class, abstract its method that is used by this code into a protocol.
@protocol CrNSCarbonMenuImpl <NSObject>

// Highlights the menu item at the provided index.
- (void)highlightItemAtIndex:(NSInteger)index;

@end

@interface NSMenu (Impl)

// Returns the impl. (If called on macOS 14 this would return a subclass of
// NSCocoaMenuImpl, but private API use is not needed on macOS 14.)
- (id<CrNSCarbonMenuImpl>)_menuImpl;

// Returns the bounds of the entire menu in screen coordinate space. Available
// on both Carbon and Cocoa impls, but always (incorrectly) returns a zero
// origin with the Cocoa impl. Therefore, do not use with macOS 14 or later.
- (CGRect)_boundsIfOpen;

@end

// --- Private API end ---

@implementation MenuControllerCocoaDelegateImpl {
  NSMutableArray* __strong _menuObservers;
  remote_cocoa::mojom::MenuControllerParamsPtr _params;
}

- (instancetype)initWithParams:
    (remote_cocoa::mojom::MenuControllerParamsPtr)params {
  if (self = [super init]) {
    _menuObservers = [[NSMutableArray alloc] init];
    _params = std::move(params);
  }
  return self;
}

- (void)dealloc {
  for (NSObject* obj in _menuObservers) {
    [NSNotificationCenter.defaultCenter removeObserver:obj];
  }
}

- (void)controllerWillAddItem:(NSMenuItem*)menuItem
                    fromModel:(ui::MenuModel*)model
                      atIndex:(size_t)index {
  if (model->IsNewFeatureAt(index)) {
    NSTextAttachment* attachment = [[NSTextAttachment alloc] initWithData:nil
                                                                   ofType:nil];
    attachment.image = NewTagImage(*_params);
    NSSize newTagSize = attachment.image.size;

    // The baseline offset of the badge image to the menu text baseline.
    const int kBadgeBaselineOffset = -3;
    attachment.bounds = NSMakeRect(0, kBadgeBaselineOffset, newTagSize.width,
                                   newTagSize.height);

    NSMutableAttributedString* attrTitle =
        [[NSMutableAttributedString alloc] initWithString:menuItem.title];
    [attrTitle
        appendAttributedString:[NSAttributedString
                                   attributedStringWithAttachment:attachment]];

    menuItem.attributedTitle = attrTitle;
  }

  if (model->IsAlertedAt(index)) {
    NSImage* iphDotImage = IPHDotImage(*_params);
    menuItem.onStateImage = iphDotImage;
    menuItem.offStateImage = iphDotImage;
    menuItem.mixedStateImage = iphDotImage;
  }
}

- (void)controllerWillAddMenu:(NSMenu*)menu fromModel:(ui::MenuModel*)model {
  std::optional<size_t> alertedIndex;

  // A map containing elements that need to be tracked, mapping from their
  // identifiers to their indexes in the menu.
  std::map<ui::ElementIdentifier, NSInteger> elementIds;

  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (model->IsAlertedAt(i)) {
      CHECK(!alertedIndex.has_value())
          << "Mac menu code can only alert for one item in a menu";
      alertedIndex = i;
    }
    const ui::ElementIdentifier identifier = model->GetElementIdentifierAt(i);
    if (identifier) {
      elementIds.emplace(identifier, base::checked_cast<NSInteger>(i));
    }
  }

  // A weak reference to the menu for the two blocks. This shouldn't be
  // necessary, as there aren't any references back that make a retain cycle,
  // but it's hard to be fully convinced that such a cycle isn't possible now or
  // in the future with updates.
  __weak NSMenu* weakMenu = menu;

  if (alertedIndex.has_value() || !elementIds.empty()) {
    __block bool menuShown = false;
    auto shownCallback = ^(NSNotification* note) {
      NSMenu* strongMenu = weakMenu;
      if (!strongMenu) {
        return;
      }

      if (@available(macOS 14.0, *)) {
        // Ensure that only notifications for the correct internal menu view
        // class trigger this.
        if (![[note.object className] isEqual:@"NSContextMenuItemView"]) {
          return;
        }

        // Ensure that the bounds for all the needed menu items are available.
        // In testing, this was always true even for the first notification, so
        // this is not expected to fail and is included for paranoia.
        for (auto [elementId, index] : elementIds) {
          NSRect frame = [strongMenu itemAtIndex:index].accessibilityFrame;
          if (NSWidth(frame) < 10) {
            return;
          }
        }
      }

      // These notifications will fire more than once; only process the first
      // time.
      if (menuShown) {
        return;
      }
      menuShown = true;

      if (alertedIndex.has_value()) {
        const auto index = base::checked_cast<NSInteger>(alertedIndex.value());
        if (@available(macOS 14.0, *)) {
          [strongMenu itemAtIndex:index].accessibilitySelected = true;
        } else {
          [strongMenu._menuImpl highlightItemAtIndex:index];
        }
      }

      if (@available(macOS 14.0, *)) {
        for (auto [elementId, index] : elementIds) {
          NSRect frame = [strongMenu itemAtIndex:index].accessibilityFrame;
          ui::ElementTrackerMac::GetInstance()->NotifyMenuItemShown(
              strongMenu, elementId, gfx::ScreenRectFromNSRect(frame));
        }
      } else {
        // macOS 13 and earlier use the old Carbon Menu Manager, and getting the
        // bounds of menus is pretty wackadoodle.
        //
        // Because menus are implemented in Carbon, the only notification that
        // can be relied upon is `NSMenuDidBeginTrackingNotification`, but that
        // is fired before layout is done. Therefore, spin the event loop once.
        // This practically guarantees that the menu is on screen and can be
        // queried for size.
        dispatch_after(
            dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC),
            dispatch_get_main_queue(), ^{
              gfx::Rect bounds =
                  gfx::ScreenRectFromNSRect(strongMenu._boundsIfOpen);
              for (auto [elementId, index] : elementIds) {
                ui::ElementTrackerMac::GetInstance()->NotifyMenuItemShown(
                    strongMenu, elementId, bounds);
              }
            });
      };
    };

    // Register for a notification to get a callback when the menu is shown.
    // `NSMenuDidBeginTrackingNotification` might seem ideal, but it fires very
    // early in menu tracking, before layout happens.
    if (@available(macOS 14.0, *)) {
      // With macOS 14+, menus are implemented with Cocoa. Because all the menu-
      // specific notifications fire very early, before layout, rely on the
      // NSViewFrameDidChangeNotification being fired for a specific menu
      // implementation class.
      [_menuObservers
          addObject:[NSNotificationCenter.defaultCenter
                        addObserverForName:NSViewFrameDidChangeNotification
                                    object:nil
                                     queue:nil
                                usingBlock:shownCallback]];
    } else {
      // Before macOS 14, menus were implemented with Carbon and only the basic
      // notifications were hooked up. Therefore, as much as
      // `NSMenuDidBeginTrackingNotification` is not ideal, register for it, and
      // play `dispatch_after` games, above.
      [_menuObservers
          addObject:[NSNotificationCenter.defaultCenter
                        addObserverForName:NSMenuDidBeginTrackingNotification
                                    object:menu
                                     queue:nil
                                usingBlock:shownCallback]];
    }
  }

  if (!elementIds.empty()) {
    auto hiddenCallback = ^(NSNotification* note) {
      NSMenu* strongMenu = weakMenu;
      if (!strongMenu) {
        return;
      }

      // We expect to see the following order of events:
      //
      // - element shown
      // - element activated (optional)
      // - element hidden
      //
      // However, the OS notification for "element activated" fires *after* the
      // NSMenuDidEndTrackingNotification notification that is used here for
      // "element hidden". Therefore, to ensure correct ordering, defer
      // processing "element hidden" by posting to the main dispatch queue.
      dispatch_after(
          dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC),
          dispatch_get_main_queue(), ^{
            for (auto [elementId, index] : elementIds) {
              ui::ElementTrackerMac::GetInstance()->NotifyMenuItemHidden(
                  strongMenu, elementId);
            }
          });
    };

    [_menuObservers
        addObject:[NSNotificationCenter.defaultCenter
                      addObserverForName:NSMenuDidEndTrackingNotification
                                  object:menu
                                   queue:nil
                              usingBlock:hiddenCallback]];
  }
}

@end
