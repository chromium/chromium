// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/profiles/batch_upload_dialog_view.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

// Testing implementation of `BatchUploadDataProvider`.
// TODO(b/362733052): Separate into its own file to be used by
// other tests with more useful functions for testing.
class BatchUploadDataProviderFake : public BatchUploadDataProvider {
 public:
  explicit BatchUploadDataProviderFake(BatchUploadDataType type)
      : BatchUploadDataProvider(type) {}

  void SetHasLocalData(bool has_local_data) {
    has_local_data_ = has_local_data;
  }

  bool HasLocalData() const override { return has_local_data_; }

  BatchUploadDataContainer GetLocalData() const override {
    BatchUploadDataContainer container(/*section_name_id=*/123,
                                       /*dialog_subtitle_id=*/456);
    if (has_local_data_) {
      // Add an arbitrary item.
      container.items.push_back({.id = BatchUploadDataItemModel::Id(123),
                                 .title = "data_title",
                                 .subtitle = "data_subtitle"});
    }
    return container;
  }

  bool MoveToAccountStorage(const std::vector<BatchUploadDataItemModel::Id>&
                                item_ids_to_move) override {
    return true;
  }

 private:
  bool has_local_data_ = false;
};

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
  BatchUploadDialogViewPixelTest()
      : fake_provider_(BatchUploadDataType::kPasswords) {}

  // DialogBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam().use_dark_theme) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
  }

  void ShowUi(const std::string& name) override {
    content::TestNavigationObserver observer{
        GURL(chrome::kChromeUIBatchUploadURL)};
    observer.StartWatchingNewWebContents();
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{}, "BatchUploadDialogView");

    fake_provider_.SetHasLocalData(true);

    BatchUploadDialogView::CreateBatchUploadDialogView(
        *browser(), /*data_providers_list=*/{&fake_provider_},
        /*complete_callback*/ base::DoNothing());

    widget_waiter.WaitIfNeededAndGet();
    observer.Wait();
  }

 private:
  BatchUploadDataProviderFake fake_provider_;

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
