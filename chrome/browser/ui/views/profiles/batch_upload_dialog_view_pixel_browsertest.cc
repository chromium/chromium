// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/i18n/number_formatting.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_data_provider.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/batch_upload_dialog_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

const gfx::Image kSignedInImage = gfx::test::CreateImage(20, 20, SK_ColorBLUE);
const char kSignedInImageUrl[] = "SIGNED_IN_IMAGE_URL";

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
      BatchUploadDataItemModel item;
      item.id = BatchUploadDataItemModel::Id(i);
      item.icon_url = GetDataType() == BatchUploadDataType::kPasswords
                          ? GURL("chrome://theme/IDR_PASSWORD_MANAGER_FAVICON")
                          : GURL();
      item.title =
          data_name + "_title_" + base::UTF16ToUTF8(base::FormatNumber(i));
      item.subtitle =
          data_name + "_subtitle_" + base::UTF16ToUTF8(base::FormatNumber(i));
      container.items.push_back(std::move(item));
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
};

struct TestParam {
  std::string test_suffix = "";
  bool use_dark_theme = false;
  std::vector<std::pair<int, BatchUploadDataType>> section_item_count_name = {
      {2, BatchUploadDataType::kPasswords},
      {1, BatchUploadDataType::kAddresses},
  };
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

    {.test_suffix = "MultipleSectionsScrollbar",
     // Multiple sections with the same type just for testing purposes.
     .section_item_count_name = {{2, BatchUploadDataType::kPasswords},
                                 {1, BatchUploadDataType::kPasswords},
                                 {10, BatchUploadDataType::kAddresses},
                                 {15, BatchUploadDataType::kAddresses},
                                 {16, BatchUploadDataType::kAddresses},
                                 {10, BatchUploadDataType::kPasswords},
                                 {5, BatchUploadDataType::kPasswords}}},
};

}  // namespace

class BatchUploadDialogViewPixelTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  BatchUploadDialogViewPixelTest() {
    for (const auto& input_section : GetParam().section_item_count_name) {
      fake_providers_.emplace_back(input_section.second, input_section.first);
    }

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

  // Gets the list of providers as a list of pointers that
  // `CreateBatchUploadDialogView` expects.
  std::vector<raw_ptr<const BatchUploadDataProvider>> GetProvidersPtrVector() {
    std::vector<raw_ptr<const BatchUploadDataProvider>> ret;
    std::ranges::transform(
        fake_providers_, std::back_inserter(ret),
        [](const BatchUploadDataProviderFake& provider) { return &provider; });
    return ret;
  }

  // DialogBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam().use_dark_theme) {
      command_line->AppendSwitch(switches::kForceDarkMode);
    }
  }

  void SigninWithFullInfo() {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    AccountInfo account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "test@gmail.com", signin::ConsentLevel::kSignin);
    ASSERT_FALSE(account_info.IsEmpty());

    account_info.full_name = "Joe Testing";
    account_info.given_name = "Joe";
    account_info.picture_url = "SOME_FAKE_URL";
    account_info.hosted_domain = kNoHostedDomainFound;
    account_info.locale = "en";
    ASSERT_TRUE(account_info.IsValid());
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);

    signin::SimulateAccountImageFetch(identity_manager, account_info.account_id,
                                      kSignedInImageUrl, kSignedInImage);
  }

  void ShowUi(const std::string& name) override {
    SigninWithFullInfo();

    content::TestNavigationObserver observer{
        GURL(chrome::kChromeUIBatchUploadURL)};
    observer.StartWatchingNewWebContents();
    views::NamedWidgetShownWaiter widget_waiter(
        views::test::AnyWidgetTestPasskey{}, "BatchUploadDialogView");

    BatchUploadDialogView::CreateBatchUploadDialogView(
        *browser(), /*data_providers_list=*/GetProvidersPtrVector(),
        /*complete_callback*/ base::DoNothing());

    widget_waiter.WaitIfNeededAndGet();
    observer.Wait();
  }

 private:
  std::vector<BatchUploadDataProviderFake> fake_providers_;

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
