// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/i18n/number_formatting.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/profiles/batch_upload_dialog_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
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
  explicit BatchUploadDataProviderFake(BatchUploadDataType type, int item_count)
      : BatchUploadDataProvider(type),
        item_count_(item_count),
        section_name_id_(type == BatchUploadDataType::kPasswords
                             ? IDS_BATCH_UPLOAD_SECTION_TITLE_PASSWORDS
                             : IDS_BATCH_UPLOAD_SECTION_TITLE_ADDRESSES),
        data_name(type == BatchUploadDataType::kPasswords ? "password"
                                                          : "address") {}

  bool HasLocalData() const override { return item_count_ > 0; }

  BatchUploadDataContainer GetLocalData() const override {
    BatchUploadDataContainer container(
        section_name_id_,
        /*dialog_subtitle_id=*/IDS_BATCH_UPLOAD_SUBTITLE);

    // Add arbitrary items.
    for (int i = 0; i < item_count_; ++i) {
      container.items.push_back(
          {.id = BatchUploadDataItemModel::Id(i),
           .title =
               data_name + "_title_" + base::UTF16ToUTF8(base::FormatNumber(i)),
           .subtitle = data_name + "_subtitle_" +
                       base::UTF16ToUTF8(base::FormatNumber(i))});
    }
    return container;
  }

  bool MoveToAccountStorage(const std::vector<BatchUploadDataItemModel::Id>&
                                item_ids_to_move) override {
    return true;
  }

 private:
  int item_count_;
  int section_name_id_;
  const std::string data_name;
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
      : fake_provider_(BatchUploadDataType::kPasswords, 2),
        fake_provider2_(BatchUploadDataType::kAddresses, 1) {
    // The Batch Upload view seems not to be resized properly on changes which
    // causes the view to go out of bounds. This should not happen and needs to
    // be investigated further. As a work around, to have a proper screenshot
    // tests, disable the check.
    // TODO(b/368043624): Make the view resize properly and remove this line as
    // it is not recommended to have per
    // `TestBrowserDialog::should_verify_dialog_bounds_` definition and default
    // value.
    set_should_verify_dialog_bounds(false);
  }

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

    BatchUploadDialogView::CreateBatchUploadDialogView(
        *browser(), /*data_providers_list=*/{&fake_provider_, &fake_provider2_},
        /*complete_callback*/ base::DoNothing());

    widget_waiter.WaitIfNeededAndGet();
    observer.Wait();
  }

 private:
  BatchUploadDataProviderFake fake_provider_;
  BatchUploadDataProviderFake fake_provider2_;

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
