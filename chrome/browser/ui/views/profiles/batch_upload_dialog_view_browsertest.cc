// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/batch_upload_dialog_view.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_controller.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

constexpr base::flat_map<BatchUploadDataType,
                         std::vector<BatchUploadDataItemModel::Id>>
    kEmptySelectedMap;

class BatchUploadDataProviderFake : public BatchUploadDataProvider {
 public:
  explicit BatchUploadDataProviderFake(BatchUploadDataType type)
      : BatchUploadDataProvider(type) {}

  void SetHasLocalData(bool has_local_data) {
    has_local_data_ = has_local_data;
  }

  bool HasLocalData() const override { return has_local_data_; }

  BatchUploadDataContainer GetLocalData() const override {
    // IDs used here are arbitrary and should not be checked.
    BatchUploadDataContainer container(
        /*section_name_id=*/IDS_BATCH_UPLOAD_SECTION_TITLE_PASSWORDS,
        /*dialog_subtitle_id=*/IDS_BATCH_UPLOAD_SUBTITLE);
    if (has_local_data_) {
      // Add an arbitrary item.
      BatchUploadDataItemModel item;
      item.id = BatchUploadDataItemModel::Id(123);
      item.title = "data_title";
      item.subtitle = "data_subtitle";
      container.items.push_back(std::move(item));
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

}  // namespace

class BatchUploadDialogViewBrowserTest : public InProcessBrowserTest {
 public:
  BatchUploadDialogView* CreateBatchUploadDialogView(
      Profile* profile,
      const std::vector<raw_ptr<const BatchUploadDataProvider>>&
          data_providers_list,
      SelectedDataTypeItemsCallback complete_callback) {
    content::TestNavigationObserver observer{
        GURL(chrome::kChromeUIBatchUploadURL)};
    observer.StartWatchingNewWebContents();

    BatchUploadDialogView* dialog_view =
        BatchUploadDialogView::CreateBatchUploadDialogView(
            *browser(), data_providers_list, std::move(complete_callback));

    observer.Wait();

    return dialog_view;
  }

  void SigninWithFullInfo() {
    signin::IdentityManager* identity_manager = GetIdentityManager();
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
  }

  void Signout() { signin::ClearPrimaryAccount(GetIdentityManager()); }

  void TriggerSigninPending() {
    signin::SetInvalidRefreshTokenForPrimaryAccount(GetIdentityManager());
  }

 private:
  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  // Needed to make sure the mojo binders are set.
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kBatchUploadDesktop};
};

IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       OpenBatchUploadDialogViewWithCloseAction) {
  SigninWithFullInfo();

  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  BatchUploadDataProviderFake fake_provider(BatchUploadDataType::kPasswords);
  fake_provider.SetHasLocalData(true);
  BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
      browser()->profile(), {&fake_provider}, mock_callback.Get());

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);
  dialog_view->OnDialogSelectionMade({});
}

IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       OpenBatchUploadDialogViewWithDestroyed) {
  SigninWithFullInfo();

  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);
  {
    BatchUploadDataProviderFake fake_provider(BatchUploadDataType::kPasswords);
    fake_provider.SetHasLocalData(true);
    BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
        browser()->profile(), {&fake_provider}, mock_callback.Get());

    // Simulate the widget closing without user action.
    views::Widget* widget = dialog_view->GetWidget();
    ASSERT_TRUE(widget);
    widget->Close();
  }
}

// Fails on Mac only.  http://crbug.com/372194892
#if BUILDFLAG(IS_MAC)
#define MAYBE_OpenBatchUploadDialogViewClosesOnSignout \
  DISABLED_OpenBatchUploadDialogViewClosesOnSignout
#else
#define MAYBE_OpenBatchUploadDialogViewClosesOnSignout \
  OpenBatchUploadDialogViewClosesOnSignout
#endif
IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       MAYBE_OpenBatchUploadDialogViewClosesOnSignout) {
  SigninWithFullInfo();

  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);

  BatchUploadDataProviderFake fake_provider(BatchUploadDataType::kPasswords);
  fake_provider.SetHasLocalData(true);
  BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
      browser()->profile(), {&fake_provider}, mock_callback.Get());
  ASSERT_TRUE(dialog_view->GetWidget()->IsVisible());

  // Signing out should close the dialog.
  Signout();

  EXPECT_FALSE(dialog_view->GetWidget()->IsVisible());
}

// Fails on Mac only.  http://crbug.com/372194892
#if BUILDFLAG(IS_MAC)
#define MAYBE_OpenBatchUploadDialogViewClosesOnSigninPending \
  DISABLED_OpenBatchUploadDialogViewClosesOnSigninPending
#else
#define MAYBE_OpenBatchUploadDialogViewClosesOnSigninPending \
  OpenBatchUploadDialogViewClosesOnSigninPending
#endif
IN_PROC_BROWSER_TEST_F(BatchUploadDialogViewBrowserTest,
                       MAYBE_OpenBatchUploadDialogViewClosesOnSigninPending) {
  SigninWithFullInfo();

  base::MockCallback<SelectedDataTypeItemsCallback> mock_callback;

  EXPECT_CALL(mock_callback, Run(kEmptySelectedMap)).Times(1);

  BatchUploadDataProviderFake fake_provider(BatchUploadDataType::kPasswords);
  fake_provider.SetHasLocalData(true);
  BatchUploadDialogView* dialog_view = CreateBatchUploadDialogView(
      browser()->profile(), {&fake_provider}, mock_callback.Get());
  ASSERT_TRUE(dialog_view->GetWidget()->IsVisible());

  // Signing out should close the dialog.
  TriggerSigninPending();

  EXPECT_FALSE(dialog_view->GetWidget()->IsVisible());
}
