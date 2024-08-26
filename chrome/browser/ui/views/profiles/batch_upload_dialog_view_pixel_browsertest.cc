// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/profiles/batch_upload_dialog_view.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

struct TestParam {
  std::string test_suffix = "";
  bool use_dark_theme = false;
};

// Allows the test to be named like
// `BatchUploadDialogViewPixelTest.InvokeUi_default/{test_suffix}`.
std::string ParamToTestSuffix(const ::testing::TestParamInfo<TestParam>& info) {
  return info.param.test_suffix;
}

// Test configurations
const TestParam kTestParams[] = {
    {.test_suffix = "Regular"},
    {.test_suffix = "DarkTheme", .use_dark_theme = true},
};

}  // namespace

class BatchUploadDialogViewPixelTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  // DialogBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam().use_dark_theme) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
  }

  void ShowUi(const std::string& name) override {
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{}, "BatchUploadDialogView");

    BatchUploadDialogView::CreateBatchUploadDialogView(*browser(), {},
                                                       base::DoNothing());

    widget_waiter.WaitIfNeededAndGet();
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kBatchUploadDesktop};
};

IN_PROC_BROWSER_TEST_P(BatchUploadDialogViewPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         BatchUploadDialogViewPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
