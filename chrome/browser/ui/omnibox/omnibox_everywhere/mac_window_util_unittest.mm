// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/mac_window_util.h"

#import <AppKit/AppKit.h>

#include "base/apple/scoped_objc_class_swizzler.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/base_window.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

// Swizzled in for -[NSApplication currentEvent] to simulate a hit test made
// when there is no current event.
@interface OmniboxEverywhereNilCurrentEventDonor : NSObject
@end

@implementation OmniboxEverywhereNilCurrentEventDonor
- (NSEvent*)currentEvent {
  return nil;
}
@end

namespace omnibox_everywhere {
namespace {

class TestBaseWindow : public ui::BaseWindow {
 public:
  explicit TestBaseWindow(NSWindow* window) : window_(window) {}
  ~TestBaseWindow() = default;

  bool IsActive() const override { return [window_ isKeyWindow]; }
  bool IsMaximized() const override { return [window_ isZoomed]; }
  bool IsMinimized() const override { return [window_ isMiniaturized]; }
  bool IsFullscreen() const override {
    return ([window_ styleMask] & NSWindowStyleMaskFullScreen) != 0;
  }
  gfx::NativeWindow GetNativeWindow() const override {
    return gfx::NativeWindow(window_);
  }
  gfx::Rect GetRestoredBounds() const override { return gfx::Rect(); }
  ui::mojom::WindowShowState GetRestoredState() const override {
    return ui::mojom::WindowShowState::kNormal;
  }
  gfx::Rect GetBounds() const override { return gfx::Rect(); }
  void Show() override { [window_ orderFront:nil]; }
  void ShowInactive() override { [window_ orderFront:nil]; }
  void Hide() override { [window_ orderOut:nil]; }
  void Close() override { [window_ close]; }
  bool IsVisible() const override { return [window_ isVisible]; }
  void SetBounds(const gfx::Rect& bounds) override {}
  void FlashFrame(bool flash) override {}
  void SetZOrderLevel(ui::ZOrderLevel order) override {}
  ui::ZOrderLevel GetZOrderLevel() const override {
    return ui::ZOrderLevel::kNormal;
  }
  void Activate() override { [window_ makeKeyAndOrderFront:nil]; }
  void Deactivate() override { [window_ resignKeyWindow]; }
  void Maximize() override {}
  void Minimize() override { [window_ miniaturize:nil]; }
  void Restore() override { [window_ deminiaturize:nil]; }

 private:
  NSWindow* __strong window_;
};

class MacWindowUtilTest : public testing::Test {
 public:
  void SetUp() override {
    nswindow_ =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 400, 300)
                                    styleMask:NSWindowStyleMaskTitled |
                                              NSWindowStyleMaskResizable |
                                              NSWindowStyleMaskMiniaturizable
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    [nswindow_ setReleasedWhenClosed:NO];
  }

  void TearDown() override {
    [nswindow_ close];
    nswindow_ = nil;
  }

 protected:
  NSWindow* nswindow_;
};

TEST_F(MacWindowUtilTest, ActivateBrowserWindowOnMacNullSafe) {
  ActivateBrowserWindowOnMac(nullptr);

  MockBrowserWindowInterface mock_bwi;
  EXPECT_CALL(mock_bwi, GetWindow()).WillRepeatedly(testing::Return(nullptr));
  ActivateBrowserWindowOnMac(&mock_bwi);
}

TEST_F(MacWindowUtilTest, ActivateBrowserWindowOrdersWindowFront) {
  TestBaseWindow test_base_window(nswindow_);
  MockBrowserWindowInterface mock_bwi;
  EXPECT_CALL(mock_bwi, GetWindow())
      .WillRepeatedly(testing::Return(&test_base_window));

  ActivateBrowserWindowOnMac(&mock_bwi);
  EXPECT_TRUE([nswindow_ isVisible]);
}

TEST_F(MacWindowUtilTest, DisassociatePopupOnMac) {
  // Null native window should be safely handled.
  DisassociatePopupOnMac(gfx::NativeWindow());

  // Real NSWindow should have its collection behavior reset to Default.
  [nswindow_
      setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces |
                            NSWindowCollectionBehaviorFullScreenAuxiliary];
  DisassociatePopupOnMac(gfx::NativeWindow(nswindow_));
  EXPECT_EQ([nswindow_ collectionBehavior], NSWindowCollectionBehaviorDefault);
}

TEST_F(MacWindowUtilTest, NonClientHitTestAllowsHoverInDraggableRegion) {
  OmniboxEverywhereWidgetDelegate delegate(/*template_url_service=*/nullptr);
  SkRegion region;
  region.op(SkIRect::MakeXYWH(0, 0, 200, 100), SkRegion::kUnion_Op);
  delegate.SetDraggableRegion(region);

  auto set_current_mouse_event = [&](NSEventType type,
                                     NSEventModifierFlags modifiers = 0) {
    NSEvent* event = [NSEvent mouseEventWithType:type
                                        location:NSMakePoint(50, 50)
                                   modifierFlags:modifiers
                                       timestamp:0
                                    windowNumber:[nswindow_ windowNumber]
                                         context:nil
                                     eventNumber:0
                                      clickCount:1
                                        pressure:1.0];
    [NSApp postEvent:event atStart:YES];
    [NSApp nextEventMatchingMask:NSEventMaskAny
                       untilDate:nil
                          inMode:NSDefaultRunLoopMode
                         dequeue:YES];
  };

  set_current_mouse_event(NSEventTypeMouseMoved);
  EXPECT_FALSE(ShouldTreatCurrentEventAsDraggableOnMac());
  EXPECT_EQ(delegate.NonClientHitTest(gfx::Point(50, 50)), HTNOWHERE);

  set_current_mouse_event(NSEventTypeLeftMouseDown, NSEventModifierFlagControl);
  EXPECT_FALSE(ShouldTreatCurrentEventAsDraggableOnMac());
  EXPECT_EQ(delegate.NonClientHitTest(gfx::Point(50, 50)), HTNOWHERE);

  set_current_mouse_event(NSEventTypeLeftMouseDown);
  EXPECT_TRUE(ShouldTreatCurrentEventAsDraggableOnMac());
  EXPECT_EQ(delegate.NonClientHitTest(gfx::Point(50, 50)), HTCAPTION);
  EXPECT_EQ(delegate.NonClientHitTest(gfx::Point(300, 300)), HTNOWHERE);
}

TEST_F(MacWindowUtilTest, NonClientHitTestIgnoresMissingCurrentEvent) {
  OmniboxEverywhereWidgetDelegate delegate(/*template_url_service=*/nullptr);
  SkRegion region;
  region.op(SkIRect::MakeXYWH(0, 0, 200, 100), SkRegion::kUnion_Op);
  delegate.SetDraggableRegion(region);

  base::apple::ScopedObjCClassSwizzler swizzler(
      [NSApplication class], [OmniboxEverywhereNilCurrentEventDonor class],
      @selector(currentEvent));
  ASSERT_FALSE([NSApp currentEvent]);

  EXPECT_FALSE(ShouldTreatCurrentEventAsDraggableOnMac());
  EXPECT_EQ(delegate.NonClientHitTest(gfx::Point(50, 50)), HTNOWHERE);
}

}  // namespace
}  // namespace omnibox_everywhere
