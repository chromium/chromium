// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/delete_address_profile_dialog_controller_impl.h"

#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {
namespace {

constexpr char kSuppressedScreenshotError[] =
    "Screenshot can only run in pixel_tests on Windows.";
constexpr char kTestEmail[] = "test@example.com";

class DeleteAddressProfileDialogControllerImplTest
    : public InteractiveBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &DeleteAddressProfileDialogControllerImplTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
  }

  void TearDownOnMainThread() override {
    InteractiveBrowserTest::TearDownOnMainThread();
    identity_test_environment_adaptor_.reset();
  }

  void OnDialogClosed(bool user_accepted_delete) {
    user_accepted_delete_ = user_accepted_delete;
  }

  auto EnsureClosedWithUserDecision(bool user_accepted_delete) {
    return CheckVariable(user_accepted_delete_, user_accepted_delete);
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(browser()->profile()));
  }

  auto ConfigureAddressSync(bool enable_address_sync) {
    return Do([this, enable_address_sync]() {
      syncer::UserSelectableTypeSet selected_sync_types =
          sync_service()->GetUserSettings()->GetSelectedTypes();
      if (enable_address_sync) {
        selected_sync_types.Put(syncer::UserSelectableType::kAutofill);
      } else {
        selected_sync_types.Remove(syncer::UserSelectableType::kAutofill);
      }
      sync_service()->GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false, selected_sync_types);
    });
  }

  auto MakePrimaryAccountAvailable() {
    return Do([this]() {
      identity_test_environment_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable(kTestEmail,
                                        signin::ConsentLevel::kSignin);
    });
  }

  auto ShowDialog(bool is_account_address_profile) {
    return Do([this, is_account_address_profile]() {
      user_accepted_delete_.reset();

      DeleteAddressProfileDialogControllerImpl::CreateForWebContents(
          web_contents());
      DeleteAddressProfileDialogControllerImpl* const controller =
          DeleteAddressProfileDialogControllerImpl::FromWebContents(
              web_contents());
      ASSERT_THAT(controller, ::testing::NotNull());
      controller->OfferDelete(
          is_account_address_profile,
          base::BindOnce(
              &DeleteAddressProfileDialogControllerImplTest::OnDialogClosed,
              base::Unretained(this)));
    });
  }

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
  }

  std::optional<bool> user_accepted_delete_;
  base::CallbackListSubscription subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
};

IN_PROC_BROWSER_TEST_F(DeleteAddressProfileDialogControllerImplTest,
                       InvokeUi_LocalProfile) {
  RunTestSequence(
      ConfigureAddressSync(/*enable_address_sync=*/false),
      ShowDialog(/*is_account_address_profile=*/false),
      // The delete dialog resides in a different context on MacOS.
      InAnyContext(Steps(
          WaitForShow(views::DialogClientView::kTopViewId),
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSuppressedScreenshotError),
          Screenshot(views::DialogClientView::kTopViewId,
                     /*screenshot_name=*/"local_address_profile",
                     /*baseline_cl=*/"4905025"))));
}

IN_PROC_BROWSER_TEST_F(DeleteAddressProfileDialogControllerImplTest,
                       InvokeUi_SyncAddressProfile) {
  RunTestSequence(
      ConfigureAddressSync(/*enable_address_sync=*/true),
      ShowDialog(/*is_account_address_profile=*/false),
      // The delete dialog resides in a different context on MacOS.
      InAnyContext(Steps(
          WaitForShow(views::DialogClientView::kTopViewId),
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSuppressedScreenshotError),
          Screenshot(views::DialogClientView::kTopViewId,
                     /*screenshot_name=*/"sync_address_profile",
                     /*baseline_cl=*/"4905025"))));
}

IN_PROC_BROWSER_TEST_F(DeleteAddressProfileDialogControllerImplTest,
                       InvokeUi_AccountAddressProfile) {
  RunTestSequence(
      ConfigureAddressSync(/*enable_address_sync=*/false),
      MakePrimaryAccountAvailable(),
      ShowDialog(/*is_account_address_profile=*/true),
      // The delete dialog resides in a different context on MacOS.
      InAnyContext(Steps(
          WaitForShow(views::DialogClientView::kTopViewId),
          SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                                  kSuppressedScreenshotError),
          Screenshot(views::DialogClientView::kTopViewId,
                     /*screenshot_name=*/"account_address_profile",
                     /*baseline_cl=*/"4905025"))));
}

IN_PROC_BROWSER_TEST_F(DeleteAddressProfileDialogControllerImplTest,
                       DialogAccepted) {
  RunTestSequence(ConfigureAddressSync(/*enable_address_sync=*/false),
                  ShowDialog(/*is_account_address_profile=*/false),
                  InAnyContext(Steps(
                      WaitForShow(views::DialogClientView::kTopViewId),
                      PressButton(views::DialogClientView::kOkButtonElementId),
                      WaitForHide(views::DialogClientView::kTopViewId))),
                  EnsureClosedWithUserDecision(/*user_accepted_delete=*/true));
}

IN_PROC_BROWSER_TEST_F(DeleteAddressProfileDialogControllerImplTest,
                       DialogDeclined) {
  RunTestSequence(
      ConfigureAddressSync(/*enable_address_sync=*/false),
      ShowDialog(/*is_account_address_profile=*/false),
      InAnyContext(
          Steps(WaitForShow(views::DialogClientView::kTopViewId),
                PressButton(views::DialogClientView::kCancelButtonElementId),
                WaitForHide(views::DialogClientView::kTopViewId))),
      EnsureClosedWithUserDecision(/*user_accepted_delete=*/false));
}

}  // namespace
}  // namespace autofill
