// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/share_menu_controller.h"

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/mac/mac_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "net/base/apple/url_conversions.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/view.h"

// Private method, used to identify instantiated services.
@interface NSSharingService (ExposeName)
@property(readonly) NSString* name;
@end

namespace {

// The reminder service doesn't have a convenient NSSharingServiceName*
// constant.
NSString* const kRemindersSharingServiceName =
    @"com.apple.reminders.RemindersShareExtension";

bool CanShare() {
  Browser* last_active_browser = chrome::FindLastActive();
  return last_active_browser &&
         last_active_browser->location_bar_model()->ShouldDisplayURL() &&
         last_active_browser->tab_strip_model()->GetActiveWebContents() &&
         last_active_browser->tab_strip_model()
             ->GetActiveWebContents()
             ->GetLastCommittedURL()
             .is_valid();
}

}  // namespace

@implementation ShareMenuController {
  // The following three ivars are provided to the system via NSSharingService
  // delegates. They're needed for the transition animation, and to provide a
  // screenshot of the shared site for services that support it.
  NSWindow* __weak _windowForShare;
  NSRect _rectForShare;
  NSImage* __strong _snapshotForShare;

  // The Reminders share extension reads title/URL from the currently active
  // activity.
  NSUserActivity* __strong _activity;
}

// NSMenuDelegate

- (BOOL)menuHasKeyEquivalent:(NSMenu*)menu
                    forEvent:(NSEvent*)event
                      target:(id*)target
                      action:(SEL*)action {
  // Load the menu if it hasn't loaded already.
  if (!menu.numberOfItems) {
    [self menuNeedsUpdate:menu];
  }
  // Per tapted@'s comment in BookmarkMenuCocoaController, it's fine
  // to return NO here if an item will handle this. This is why it's
  // necessary to ensure the menu is loaded above.
  return NO;
}

- (void)menuNeedsUpdate:(NSMenu*)menu {
  [menu removeAllItems];

  // Using a real URL instead of empty string to avoid system log about relative
  // URLs in the pasteboard. This URL will not actually be shared to, just used
  // to fetch sharing services that can handle the NSURL type.
  NSArray* services = [NSSharingService
      sharingServicesForItems:@[ [NSURL URLWithString:@"https://google.com"] ]];
  for (NSSharingService* service in services) {
    // Don't include "Add to Reading List".
    if ([service.name
            isEqualToString:NSSharingServiceNameAddToSafariReadingList])
      continue;
    NSMenuItem* item = [self menuItemForService:service];
    [menu addItem:item];
  }
  NSMenuItem* moreItem = [[NSMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_SHARING_MORE_MAC)
             action:@selector(openSharingPrefs:)
      keyEquivalent:@""];
  moreItem.target = self;
  moreItem.image = [self moreImage];
  [menu addItem:moreItem];
}

// NSMenuItemValidation

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  if (menuItem.action == @selector(openSharingPrefs:)) {
    return YES;
  }

  return CanShare();
}

// NSSharingServiceDelegate

- (void)sharingService:(NSSharingService*)service
         didShareItems:(NSArray*)items {
  UMA_HISTOGRAM_BOOLEAN("Mac.FileMenuNativeShare", true);
  [self clearTransitionData];
}

- (void)sharingService:(NSSharingService*)service
    didFailToShareItems:(NSArray*)items
                  error:(NSError*)error {
  UMA_HISTOGRAM_BOOLEAN("Mac.FileMenuNativeShare", false);
  [self clearTransitionData];
}

- (NSRect)sharingService:(NSSharingService*)service
    sourceFrameOnScreenForShareItem:(id)item {
  return _rectForShare;
}

- (NSWindow*)sharingService:(NSSharingService*)service
    sourceWindowForShareItems:(NSArray*)items
          sharingContentScope:(NSSharingContentScope*)scope {
  *scope = NSSharingContentScopeFull;
  return _windowForShare;
}

- (NSImage*)sharingService:(NSSharingService*)service
    transitionImageForShareItem:(id)item
                    contentRect:(NSRect*)contentRect {
  return _snapshotForShare;
}

// Private methods

// Saves details required by delegate methods for the transition animation, and
// calls the provided closure when done.
- (void)saveTransitionDataFromBrowser:(Browser*)browser
                         whenComplete:(base::OnceClosure)closure {
  _windowForShare = browser->window()->GetNativeWindow().GetNativeNSWindow();
  BrowserView* browserView = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browserView) {
    return;
  }

  views::View* contentsView = browserView->contents_container();
  if (!contentsView) {
    return;
  }

  gfx::Rect screenRect = contentsView->bounds();
  views::View::ConvertRectToScreen(browserView, &screenRect);

  _rectForShare = ScreenRectToNSRect(screenRect);

  gfx::Rect rectInWidget =
      browserView->ConvertRectToWidget(contentsView->bounds());
  ui::GrabWindowSnapshot(_windowForShare, rectInWidget,
                         base::BindOnce(
                             [](ShareMenuController* controller,
                                base::OnceClosure closure, gfx::Image image) {
                               if (!image.IsEmpty()) {
                                 controller->_snapshotForShare =
                                     image.ToNSImage();
                               }
                               std::move(closure).Run();
                             },
                             self, std::move(closure)));
}

- (void)clearTransitionData {
  _windowForShare = nil;
  _rectForShare = NSZeroRect;
  _snapshotForShare = nil;
  [_activity invalidate];
  _activity = nil;
}

// Performs the share action using the sharing service represented by |sender|.
- (void)performShare:(NSMenuItem*)sender {
  CHECK(CanShare());
  Browser* browser = chrome::FindLastActive();
  CHECK(browser);

  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  CHECK(contents);
  NSURL* url = net::NSURLWithGURL(contents->GetLastCommittedURL());
  NSString* title = base::SysUTF16ToNSString(contents->GetTitle());

  NSSharingService* service =
      base::apple::ObjCCastStrict<NSSharingService>(sender.representedObject);
  service.delegate = self;
  service.subject = title;

  if ([service.name isEqual:kRemindersSharingServiceName]) {
    _activity = [[NSUserActivity alloc]
        initWithActivityType:NSUserActivityTypeBrowsingWeb];
    // webpageURL must be http or https or an exception is thrown.
    if ([url.scheme hasPrefix:@"http"]) {
      _activity.webpageURL = url;
    }
    _activity.title = title;
    [_activity becomeCurrent];
  }
  base::RunLoop run_loop;
  auto done = run_loop.QuitClosure();
  [self saveTransitionDataFromBrowser:browser
                         whenComplete:base::BindOnce(^{
                           [service performWithItems:@[ url ]];
                           std::move(done).Run();
                         })];
  run_loop.Run();
}

// Opens the "Sharing" subpane of the "Extensions" macOS preference pane.
- (void)openSharingPrefs:(NSMenuItem*)sender {
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_Extensions_Sharing);
}

// Returns the image to be used for the "More..." menu item, or nil on macOS
// version where this private method is unsupported.
- (NSImage*)moreImage {
  if ([NSSharingServicePicker
          respondsToSelector:@selector(sharedMoreMenuImage)]) {
    return
        [NSSharingServicePicker performSelector:@selector(sharedMoreMenuImage)];
  }
  return nil;
}

// Creates a menu item that calls |service| when invoked.
- (NSMenuItem*)menuItemForService:(NSSharingService*)service {
  BOOL isMail = [service.name isEqual:NSSharingServiceNameComposeEmail];
  NSString* keyEquivalent = isMail ? [self keyEquivalentForMail] : @"";
  NSString* title = isMail ? l10n_util::GetNSString(IDS_EMAIL_LINK_MAC)
                           : service.menuItemTitle;
  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                action:@selector(performShare:)
                                         keyEquivalent:keyEquivalent];
  item.target = self;
  item.image = service.image;
  item.representedObject = service;
  return item;
}

- (NSString*)keyEquivalentForMail {
  ui::Accelerator accelerator;
  bool found = GetDefaultMacAcceleratorForCommandId(IDC_EMAIL_PAGE_LOCATION,
                                                    &accelerator);
  DCHECK(found);
  return GetKeyEquivalentAndModifierMaskFromAccelerator(accelerator)
      .keyEquivalent;
}

@end
