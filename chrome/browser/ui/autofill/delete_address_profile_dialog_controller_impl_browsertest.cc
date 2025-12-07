// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/delete_address_profile_dialog_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

using ::testing::_;

class DeleteAddressProfileDialogControllerImplBrowserTest
    : public InProcessBrowserTest {
 public:
  static constexpr char const* kUserEmail = "example@gmail.com";

  DeleteAddressProfileDialogControllerImplBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(GetProfile());
    DeleteAddressProfileDialogControllerImpl::CreateForWebContents(
        web_contents());
    ASSERT_THAT(controller(), ::testing::NotNull());
    controller()->SetViewFactoryForTest(/*view_factory=*/base::DoNothing());
  }

  void TearDownOnMainThread() override {
    identity_test_env_adaptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  DeleteAddressProfileDialogControllerImpl* controller() {
    return DeleteAddressProfileDialogControllerImpl::FromWebContents(
        web_contents());
  }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(GetProfile()));
  }

  void ConfigureAddressSync(bool enable_address_sync) {
    syncer::UserSelectableTypeSet selected_sync_types =
        sync_service()->GetUserSettings()->GetSelectedTypes();
    if (enable_address_sync) {
      selected_sync_types.Put(syncer::UserSelectableType::kAutofill);
    } else {
      selected_sync_types.Remove(syncer::UserSelectableType::kAutofill);
    }
    sync_service()->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false, selected_sync_types);
  }

  void SigninUser() {
    identity_test_env_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable(kUserEmail,
                                      signin::ConsentLevel::kSignin);
  }

  void TestUiStringsHaveExpectedValues(
      const std::u16string& delete_confirmation_text) {
    EXPECT_EQ(l10n_util::GetStringUTF16(
                  IDS_SETTINGS_ADDRESS_REMOVE_CONFIRMATION_TITLE),
              controller()->GetTitle());
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_ADDRESS_REMOVE),
              controller()->GetAcceptButtonText());
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_CANCEL),
              controller()->GetDeclineButtonText());
    EXPECT_EQ(delete_confirmation_text,
              controller()->GetDeleteConfirmationText());
  }

  base::MockOnceCallback<void(bool)> delete_dialog_callback_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  autofill::test::AutofillBrowserTestEnvironment autofill_test_environment_;
};

IN_PROC_BROWSER_TEST_F(DeleteAddressProfileDialogControllerImplBrowserTest,
                       LocalAddressProfile_AssertStrings) {
  ConfigureAddressSync(/*enable_address_sync=*/false);
  controller()->OfferDelete(/*is_account_address_profile=*/false,
                            /*delete_dialog_callback=*/base::DoNothing());

  TestUiStringsHaveExpectedValues(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DELETE_LOCAL_ADDRESS_RECORD_TYPE_NOTICE));
}

IN_PROC_BROWSER_TEST_F(DeleteAddressProfileDialogControllerImplBrowserTest,
                       AccountAddressProfile_AssertStrings) {
  ConfigureAddressSync(/*enable_address_sync=*/false);
  SigninUser();
  controller()->OfferDelete(/*is_account_address_profile=*/true,
                            /*delete_dialog_callback=*/base::DoNothing());

  TestUiStringsHaveExpectedValues(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_DELETE_ACCOUNT_ADDRESS_RECORD_TYPE_NOTICE,
      base::ASCIIToUTF16(kUserEmail)));
}

IN_PROC_BROWSER_TEST_F(DeleteAddressProfileDialogControllerImplBrowserTest,
                       SyncAddressProfile_AssertStrings) {
  ConfigureAddressSync(/*enable_address_sync=*/true);
  controller()->OfferDelete(/*is_account_address_profile=*/false,
                            /*delete_dialog_callback=*/base::DoNothing());

  TestUiStringsHaveExpectedValues(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DELETE_SYNC_ADDRESS_RECORD_TYPE_NOTICE));
}

IN_PROC_BROWSER_TEST_F(DeleteAddressProfileDialogControllerImplBrowserTest,
                       AcceptedDelete) {
  ConfigureAddressSync(/*enable_address_sync=*/false);
  controller()->OfferDelete(/*is_account_address_profile=*/false,
                            delete_dialog_callback_.Get());

  EXPECT_CALL(delete_dialog_callback_, Run(true));
  controller()->OnAccepted();
  controller()->OnDialogDestroying();
}

IN_PROC_BROWSER_TEST_F(DeleteAddressProfileDialogControllerImplBrowserTest,
                       CanceledDelete) {
  ConfigureAddressSync(/*enable_address_sync=*/false);
  controller()->OfferDelete(/*is_account_address_profile=*/false,
                            delete_dialog_callback_.Get());

  EXPECT_CALL(delete_dialog_callback_, Run(false));
  controller()->OnCanceled();
  controller()->OnDialogDestroying();
}

IN_PROC_BROWSER_TEST_F(DeleteAddressProfileDialogControllerImplBrowserTest,
                       ClosedDialog) {
  ConfigureAddressSync(/*enable_address_sync=*/false);
  controller()->OfferDelete(/*is_account_address_profile=*/false,
                            delete_dialog_callback_.Get());

  EXPECT_CALL(delete_dialog_callback_, Run(false));
  controller()->OnClosed();
  controller()->OnDialogDestroying();
}

IN_PROC_BROWSER_TEST_F(DeleteAddressProfileDialogControllerImplBrowserTest,
                       UserDecisionIsReset) {
  ConfigureAddressSync(/*enable_address_sync=*/false);
  controller()->OfferDelete(/*is_account_address_profile=*/false,
                            delete_dialog_callback_.Get());

  EXPECT_CALL(delete_dialog_callback_, Run(false));
  controller()->OnClosed();
  controller()->OnDialogDestroying();

  // Show dialog for the second time without providing the user decision.
  controller()->OfferDelete(/*is_account_address_profile=*/false,
                            delete_dialog_callback_.Get());
  controller()->OnDialogDestroying();
}

IN_PROC_BROWSER_TEST_F(DeleteAddressProfileDialogControllerImplBrowserTest,
                       TabClosed) {
  ConfigureAddressSync(/*enable_address_sync=*/false);
  controller()->OfferDelete(/*is_account_address_profile=*/false,
                            delete_dialog_callback_.Get());

  EXPECT_CALL(delete_dialog_callback_, Run).Times(0);
  web_contents()->Close();
}

}  // namespace autofill
