// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/first_run_dialog_controller.h"
#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/test/view_tree_validator.h"
#include "ui/base/ui_base_switches.h"

using FirstRunDialogControllerTest = CocoaTest;
using TestController = base::scoped_nsobject<FirstRunDialogViewController>;

TestController MakeTestController(BOOL stats) {
  return TestController([[FirstRunDialogViewController alloc]
      initWithStatsCheckboxInitiallyChecked:stats]);
}

NSView* FindBrowserButton(NSView* view) {
  for (NSView* subview : [view subviews]) {
    if (![subview isKindOfClass:[NSButton class]])
      continue;
    NSString* title = [(NSButton*)subview title];
    if ([title rangeOfString:@"browser"].location != NSNotFound)
      return subview;
  }
  return nil;
}

TEST(FirstRunDialogControllerTest, SetStatsDefault) {
  TestController controller(MakeTestController(YES));
  [controller view];  // Make sure view is actually loaded.
  EXPECT_TRUE([controller isStatsReportingEnabled]);
}

TEST(FirstRunDialogControllerTest, MakeDefaultBrowserDefault) {
  TestController controller(MakeTestController(YES));
  [controller view];
  EXPECT_TRUE([controller isMakeDefaultBrowserEnabled]);
}

TEST(FirstRunDialogControllerTest, ShowBrowser) {
  TestController controller(MakeTestController(YES));
  NSView* checkbox = FindBrowserButton([controller view]);
  EXPECT_FALSE(checkbox.hidden);
}

TEST(FirstRunDialogControllerTest, LayoutWithLongStrings) {
  // It's necessary to call |view| on the controller before mangling the
  // strings, since otherwise the controller will lazily construct its view,
  // which might happen after the call to |set_mangle_localized_strings|.
  TestController defaultController(MakeTestController(YES));
  NSView* defaultView = [defaultController view];

  ui::ResourceBundle::GetSharedInstance().set_mangle_localized_strings_for_test(
      true);
  TestController longController(MakeTestController(YES));
  NSView* longView = [longController view];

  // Ensure that the mangled strings actually do change the height!
  EXPECT_NE(defaultView.frame.size.height, longView.frame.size.height);

  absl::optional<ui::ViewTreeProblemDetails> details =
      ui::ValidateViewTree(longView);

  EXPECT_FALSE(details.has_value()) << details->ToString();
}
