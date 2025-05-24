// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/i18n/number_formatting.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/batch_upload_dialog_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/local_data_description.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/widget/any_widget_observer.h"

using signin::constants::kNoHostedDomainFound;

namespace {

const gfx::Image kSignedInImage = gfx::test::CreateImage(20, 20, SK_ColorBLUE);
const char kSignedInImageUrl[] = "SIGNED_IN_IMAGE_URL";

syncer::LocalDataDescription GetFakeLocalData(syncer::DataType type,
                                              int item_count,
                                              bool long_title) {
  syncer::LocalDataDescription data_descriptions;
  data_descriptions.type = type;

  const std::string data_name =
      type == syncer::DataType::PASSWORDS ? "password" : "address";
  // Add arbitrary items.
  for (int i = 0; i < item_count; ++i) {
    syncer::LocalDataItemModel item;
    item.id = syncer::LocalDataItemModel::DataId(base::ToString(i));

    switch (type) {
      case syncer::DataType::PASSWORDS:
        item.icon = syncer::LocalDataItemModel::PageUrlIcon(
            GURL("https://chromium.org"));
        break;
      case syncer::DataType::BOOKMARKS:
        item.icon = syncer::LocalDataItemModel::FolderIcon();
        break;
      case syncer::DataType::CONTACT_INFO:
      case syncer::DataType::THEMES:
        item.icon = syncer::LocalDataItemModel::NoIcon();
        break;
      default:
        NOTREACHED();
    }

    // Theme item has a different title as it is shown in the section title.
    if (type == syncer::DataType::THEMES) {
      item.title = long_title ? "Custom theme name that is very very long long "
                                "long long long long"
                              : "Custom theme name";
    } else {
      item.title =
          data_name + "_title_" + base::UTF16ToUTF8(base::FormatNumber(i));
      item.subtitle =
          data_name + "_subtitle_" + base::UTF16ToUTF8(base::FormatNumber(i));
    }

    data_descriptions.local_data_models.push_back(std::move(item));
  }
  return data_descriptions;
}

struct TestParam {
  std::string test_suffix = "";
  bool use_dark_theme = false;
  std::vector<std::pair<int, syncer::DataType>> section_item_count_type = {
      {2, syncer::DataType::PASSWORDS},
      {1, syncer::DataType::CONTACT_INFO},
  };
  // Currently only relevant for Themes section as the item title is shown in
  // the section title.
  bool long_title = false;
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
     .section_item_count_type = {{2, syncer::DataType::PASSWORDS},
                                 {1, syncer::DataType::PASSWORDS},
                                 {10, syncer::DataType::CONTACT_INFO},
                                 {15, syncer::DataType::CONTACT_INFO},
                                 {16, syncer::DataType::CONTACT_INFO},
                                 {10, syncer::DataType::PASSWORDS},
                                 {5, syncer::DataType::PASSWORDS}}},

    // Hero type means the type will be shown in the subtitle of the dialog.
    // Password is a hero type.
    {.test_suffix = "SingleSectionPasswordsHeroTypeWithOneItem",
     .section_item_count_type = {{1, syncer::DataType::PASSWORDS}}},
    {.test_suffix = "SingleSectionPasswordsHeroTypeWithMultipleItems",
     .section_item_count_type = {{5, syncer::DataType::PASSWORDS}}},

    // Hero type with multiple sections. Should show "and other items" in the
    // subtitle of the dialog.
    {.test_suffix = "MultipleSectionsPasswordsHeroTypeWithOneItem",
     .section_item_count_type = {{1, syncer::DataType::PASSWORDS},
                                 {3, syncer::DataType::CONTACT_INFO}}},
    {.test_suffix = "MultipleSectionsPasswordsHeroTypeWithMultipleItems",
     .section_item_count_type = {{5, syncer::DataType::PASSWORDS},
                                 {3, syncer::DataType::CONTACT_INFO}}},

    // Bookmarks hero type.
    {.test_suffix = "SingleSectionBookmarksHeroTypeWithOneItem",
     .section_item_count_type = {{1, syncer::DataType::BOOKMARKS}}},
    {.test_suffix = "SingleSectionBookmarksHeroTypeWithMultipleItems",
     .section_item_count_type = {{5, syncer::DataType::BOOKMARKS}}},
    {.test_suffix = "MultipleSectionsBookmarksHeroTypeWithOneItem",
     .section_item_count_type = {{1, syncer::DataType::BOOKMARKS},
                                 {3, syncer::DataType::CONTACT_INFO}}},
    {.test_suffix = "MultipleSectionsBookmarksHeroTypeWithMultipleItems",
     .section_item_count_type = {{5, syncer::DataType::BOOKMARKS},
                                 {3, syncer::DataType::CONTACT_INFO}}},

    // Addresses is not a hero type. It should not show in the subtitle.
    {.test_suffix = "SingleSectionNonHeroTypeWithOneItem",
     .section_item_count_type = {{1, syncer::DataType::CONTACT_INFO}}},
    {.test_suffix = "SingleSectionNonHeroTypeWithMultipleItems",
     .section_item_count_type = {{5, syncer::DataType::CONTACT_INFO}}},

    // Addresses is not a hero type. It should not show in the subtitle even if
    // other hero types exists.
    {.test_suffix = "MultipleSectionsWithNonHeroTypeAsPrimarySection",
     .section_item_count_type = {{5, syncer::DataType::CONTACT_INFO},
                                 {5, syncer::DataType::PASSWORDS}}},

    // Themes data type section has a specific UI.
    {.test_suffix = "ThemesSectionOnly",
     .section_item_count_type = {{1, syncer::DataType::THEMES}}},
    {.test_suffix = "ThemesSectionOnlyWithLongName",
     .section_item_count_type = {{1, syncer::DataType::THEMES}},
     .long_title = true},
    {.test_suffix = "MultipleSectionsWithThemes",
     .section_item_count_type = {{5, syncer::DataType::CONTACT_INFO},
                                 {5, syncer::DataType::PASSWORDS},
                                 {1, syncer::DataType::THEMES}}},
    {.test_suffix = "MultipleSectionsWithThemesLongName",
     .section_item_count_type = {{5, syncer::DataType::CONTACT_INFO},
                                 {5, syncer::DataType::PASSWORDS},
                                 {1, syncer::DataType::THEMES}},
     .long_title = true},
};

}  // namespace

class BatchUploadDialogViewPixelTest
    : public DialogBrowserTest,
      public testing::WithParamInterface<TestParam> {
 public:
  BatchUploadDialogViewPixelTest() {
    for (const auto& [count, type] : GetParam().section_item_count_type) {
      fake_descriptions_.emplace_back(
          GetFakeLocalData(type, count, GetParam().long_title));
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
        *browser(), fake_descriptions_,
        BatchUploadService::EntryPoint::kPasswordManagerSettings,
        /*complete_callback*/ base::DoNothing());

    widget_waiter.WaitIfNeededAndGet();
    observer.Wait();
  }

 private:
  std::vector<syncer::LocalDataDescription> fake_descriptions_;
};

IN_PROC_BROWSER_TEST_P(BatchUploadDialogViewPixelTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(,
                         BatchUploadDialogViewPixelTest,
                         testing::ValuesIn(kTestParams),
                         &ParamToTestSuffix);
