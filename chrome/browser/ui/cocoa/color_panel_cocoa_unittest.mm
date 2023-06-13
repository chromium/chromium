// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/color_chooser_mac.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "mojo/core/embedder/embedder.h"
#include "skia/ext/skia_utils_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NSColorPanel (Private)
// Private method returning the NSColorPanel's target.
- (id)__target;
@end

namespace {

class ColorPanelCocoaTest : public CocoaTest {
  void SetUp() override {
    mojo::core::Init();
    // Create the color panel and call Init() again to update its initial
    // window list to include it. The NSColorPanel cannot be dealloced, so
    // without this step the tests will fail complaining that not all
    // windows were closed.
    [NSColorPanel.sharedColorPanel makeKeyAndOrderFront:nil];
    MarkCurrentWindowsAsInitial();
  }
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ColorPanelCocoaTest, ClearTargetOnEnd) {
  NSColorPanel* nscolor_panel = NSColorPanel.sharedColorPanel;
  @autoreleasepool {
    ASSERT_TRUE([nscolor_panel respondsToSelector:@selector(__target)]);
    EXPECT_FALSE([nscolor_panel __target]);

    // Create a ColorPanelCocoa.
    base::RunLoop run_loop;
    std::unique_ptr<ColorChooserMac> color_chooser_mac =
        ColorChooserMac::Create(nullptr, SK_ColorBLACK, run_loop.QuitClosure());
    run_loop.Run();

    // Confirm the NSColorPanel's configuration by the ColorChooserMac's
    // ColorPanelCocoa.
    EXPECT_TRUE([nscolor_panel __target]);

    // Release the ColorPanelCocoa.
    color_chooser_mac->End();
    color_chooser_mac.reset();
  }
}

TEST_F(ColorPanelCocoaTest, SetColor) {
  // Set the NSColor panel up with an initial color.
  NSColor* blue_color = NSColor.blueColor;
  NSColorPanel* nscolor_panel = NSColorPanel.sharedColorPanel;
  nscolor_panel.color = blue_color;
  EXPECT_TRUE([nscolor_panel.color isEqual:blue_color]);

  // Create a ColorChooserMac and confirm the NSColorPanel gets its initial
  // color.
  SkColor initial_color = SK_ColorBLACK;
  base::RunLoop run_loop_create;
  std::unique_ptr<ColorChooserMac> color_chooser_mac = ColorChooserMac::Create(
      nullptr, SK_ColorBLACK, run_loop_create.QuitClosure());
  run_loop_create.Run();

  EXPECT_NSEQ(nscolor_panel.color, skia::SkColorToDeviceNSColor(initial_color));

  // Confirm that -[ColorPanelCocoa setColor:] sets the NSColorPanel's color.
  SkColor test_color = SK_ColorRED;
  base::RunLoop run_loop_set;
  color_chooser_mac->SetSelectedColor(test_color, run_loop_set.QuitClosure());
  run_loop_set.Run();

  EXPECT_NSEQ(nscolor_panel.color, skia::SkColorToDeviceNSColor(test_color));

  // Clean up.
  color_chooser_mac->End();
  color_chooser_mac.reset();
}

}  // namespace
