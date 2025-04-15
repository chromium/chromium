// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/feature_first_run/feature_first_run_helper.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace feature_first_run {

using FeatureFirstRunDialogHelperBrowserTest = InProcessBrowserTest;

// class FeatureFirstRunDialogHelperBrowserTest : public InProcessBrowserTest {
//  public:
//   FeatureFirstRunDialogHelperBrowserTest() = default;
//   ~FeatureFirstRunDialogHelperBrowserTest() override = default;

//   content::WebContents* GetActiveWebContents() {
//     return browser()->tab_strip_model()->GetActiveWebContents();
//   }
// };

IN_PROC_BROWSER_TEST_F(FeatureFirstRunDialogHelperBrowserTest,
                       DialogConstructedFromParams) {
  const std::u16string title = u"Test Title";
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* dialog_widget = ShowFeatureFirstRunDialog(title, web_contents);
  auto* dialog_widget_delegate = dialog_widget->widget_delegate();

  EXPECT_TRUE(dialog_widget->IsVisible());
  EXPECT_EQ(title, dialog_widget_delegate->GetWindowTitle());
}

}  // namespace feature_first_run
