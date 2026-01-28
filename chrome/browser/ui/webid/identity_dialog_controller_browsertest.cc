// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/actor/ui/test_support/mock_event_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using actor::TaskId;
using testing::_;
using testing::SaveArg;

namespace {

std::vector<content::IdentityRequestDialogDisclosureField>
GetDefaultPermissions() {
  return {content::IdentityRequestDialogDisclosureField::kName,
          content::IdentityRequestDialogDisclosureField::kEmail,
          content::IdentityRequestDialogDisclosureField::kPicture};
}

}  // namespace

constexpr char kAccountId[] = "account_id1";
constexpr char16_t kTopFrameEtldPlusOne[] = u"top-frame-example.com";
constexpr char kIdpEtldPlusOne[] = "idp-example.com";

// Mock version of AccountSelectionView for injection during tests.
class MockAccountSelectionView : public AccountSelectionView {
 public:
  MockAccountSelectionView() : AccountSelectionView(/*delegate=*/nullptr) {}
  ~MockAccountSelectionView() override = default;

  MockAccountSelectionView(const MockAccountSelectionView&) = delete;
  MockAccountSelectionView& operator=(const MockAccountSelectionView&) = delete;

  MOCK_METHOD(
      bool,
      Show,
      (const content::RelyingPartyData& rp_data,
       const std::vector<IdentityProviderDataPtr>& identity_provider_data,
       const std::vector<IdentityRequestAccountPtr>& accounts,
       blink::mojom::RpMode rp_mode,
       const std::vector<IdentityRequestAccountPtr>& new_accounts),
      (override));

  MOCK_METHOD(bool,
              ShowFailureDialog,
              (const content::RelyingPartyData& rp_data,
               const std::string& idp_for_display,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               const content::IdentityProviderMetadata& idp_metadata),
              (override));

  MOCK_METHOD(bool,
              ShowErrorDialog,
              (const content::RelyingPartyData& rp_data,
               const std::string& idp_for_display,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode,
               const content::IdentityProviderMetadata& idp_metadata,
               const std::optional<TokenError>& error),
              (override));

  MOCK_METHOD(bool,
              ShowLoadingDialog,
              (const content::RelyingPartyData& rp_data,
               const std::string& idp_for_display,
               blink::mojom::RpContext rp_context,
               blink::mojom::RpMode rp_mode),
              (override));

  MOCK_METHOD(bool,
              ShowVerifyingDialog,
              (const content::RelyingPartyData&,
               const IdentityProviderDataPtr&,
               const IdentityRequestAccountPtr&,
               Account::SignInMode sign_in_mode,
               blink::mojom::RpMode),
              (override));

  MOCK_METHOD(void, SetCanShowWidget, (bool can_show_widget), (override));

  MOCK_METHOD(std::string, GetTitle, (), (const, override));

  MOCK_METHOD(std::optional<std::string>, GetSubtitle, (), (const, override));

  MOCK_METHOD(void, ShowUrl, (LinkType type, const GURL& url), (override));

  MOCK_METHOD(content::WebContents*,
              ShowModalDialog,
              (const GURL& url, blink::mojom::RpMode rp_mode),
              (override));

  MOCK_METHOD(void, CloseModalDialog, (), (override));

  MOCK_METHOD(content::WebContents*, GetRpWebContents, (), (override));
};

class MockActorTaskDelegate : public actor::ActorTaskDelegate {
 public:
  MockActorTaskDelegate() = default;
  ~MockActorTaskDelegate() override = default;

  MOCK_METHOD(void,
              OnTabAddedToTask,
              (TaskId task_id, const tabs::TabInterface::Handle& tab_handle),
              (override));

  MOCK_METHOD(void,
              RequestToShowCredentialSelectionDialog,
              (TaskId task_id,
               (const base::flat_map<std::string, gfx::Image>&)icons,
               const std::vector<actor_login::Credential>& credentials,
               CredentialSelectedCallback callback),
              (override));

  MOCK_METHOD(void,
              RequestToShowUserConfirmationDialog,
              (TaskId task_id,
               const url::Origin& navigation_origin,
               bool for_blocklisted_origin,
               UserConfirmationDialogCallback callback),
              (override));

  MOCK_METHOD(void,
              RequestToConfirmNavigation,
              (TaskId task_id,
               const url::Origin& navigation_origin,
               NavigationConfirmationCallback callback),
              (override));

  MOCK_METHOD(void,
              RequestToShowAutofillSuggestionsDialog,
              (TaskId task_id,
               std::vector<autofill::ActorFormFillingRequest> requests,
               AutofillSuggestionSelectedCallback callback),
              (override));

  base::WeakPtr<MockActorTaskDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockActorTaskDelegate> weak_factory_{this};
};

class IdentityDialogControllerBrowserTest : public InProcessBrowserTest {
 public:
  IdentityDialogControllerBrowserTest() = default;
  ~IdentityDialogControllerBrowserTest() override = default;
  IdentityDialogControllerBrowserTest(IdentityDialogControllerBrowserTest&) =
      delete;
  IdentityDialogControllerBrowserTest& operator=(
      IdentityDialogControllerBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  }

  void TearDownOnMainThread() override {
    web_contents_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::vector<IdentityRequestAccountPtr> CreateAccount() {
    return {base::MakeRefCounted<Account>(
        kAccountId, "", "", "", "", "", GURL(), "", "",
        /*potentially_approved_origin_hashes=*/std::vector<std::string>(),
        /*login_hints=*/std::vector<std::string>(),
        /*domain_hints=*/std::vector<std::string>(),
        /*labels=*/std::vector<std::string>(),
        /*idp_claimed_login_state=*/
        content::IdentityRequestAccount::LoginState::kSignUp,
        /*browser_trusted_login_state=*/
        content::IdentityRequestAccount::LoginState::kSignUp)};
  }

  IdentityProviderDataPtr CreateIdentityProviderData(
      std::vector<IdentityRequestAccountPtr>& accounts) {
    IdentityProviderDataPtr idp_data =
        base::MakeRefCounted<content::IdentityProviderData>(
            kIdpEtldPlusOne, content::IdentityProviderMetadata(),
            content::ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
            blink::mojom::RpContext::kSignIn, /*format=*/std::nullopt,
            GetDefaultPermissions(),
            /*has_login_status_mismatch=*/false);
    for (auto& account : accounts) {
      account->identity_provider = idp_data;
    }
    return idp_data;
  }

 protected:
  raw_ptr<content::WebContents> web_contents_;
  testing::NiceMock<MockActorTaskDelegate> mock_actor_task_delegate_;

  TaskId SimulateNewActiveActorTask() {
    // ExecutionEngine & ActorTask use separate actor::ui::UiEventDispatcher
    // objects, so we create separate mocks for each.
    std::unique_ptr<actor::ui::UiEventDispatcher> ui_event_dispatcher =
        actor::ui::NewMockUiEventDispatcher();
    std::unique_ptr<actor::ui::UiEventDispatcher> task_ui_event_dispatcher =
        actor::ui::NewMockUiEventDispatcher();
    auto execution_engine = actor::ExecutionEngine::CreateForTesting(
        browser()->profile(), std::move(ui_event_dispatcher));
    auto task = std::make_unique<actor::ActorTask>(
        browser()->profile(), std::move(execution_engine),
        std::move(task_ui_event_dispatcher),
        /*options=*/nullptr, mock_actor_task_delegate_.GetWeakPtr());
    tabs::TabInterface* tab =
        tabs::TabInterface::GetFromContents(web_contents_);
    EXPECT_NE(tab, nullptr);
    task->AddTab(tab->GetHandle(), base::DoNothing());
    actor::ActorKeyedService* actor_service =
        actor::ActorKeyedService::Get(browser()->profile());
    EXPECT_NE(actor_service, nullptr);
    TaskId task_id = actor_service->AddActiveTask(std::move(task));
    return task_id;
  }

  bool HasActingTaskId(IdentityDialogController* controller) {
    return !controller->acting_task_id_.is_null();
  }

  void SimulateActorTaskFinished(IdentityDialogController* controller,
                                 TaskId task_id) {
    actor::ActorKeyedService* actor_service =
        actor::ActorKeyedService::Get(browser()->profile());
    EXPECT_NE(actor_service, nullptr);
    actor_service->StopTask(task_id,
                            actor::ActorTask::StoppedReason::kTaskComplete);
    // controller->OnActorTaskStateChanged(task_id, state);
  }
};

IN_PROC_BROWSER_TEST_F(IdentityDialogControllerBrowserTest,
                       ActorTaskStateChangesCanShowWidget) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents_);
  auto mock_view = std::make_unique<MockAccountSelectionView>();
  MockAccountSelectionView* view_ptr = mock_view.get();
  controller->SetAccountSelectionViewForTesting(std::move(mock_view));

  // Simulate an actor task starting and acting on the page.
  EXPECT_CALL(*view_ptr, SetCanShowWidget(false)).Times(1);
  TaskId task_id = SimulateNewActiveActorTask();

  // Simulate the actor task finishing. This should restore visibility.
  // We expect SetCanShowWidget(true) to be called.
  EXPECT_CALL(*view_ptr, SetCanShowWidget(true)).Times(1);
  SimulateActorTaskFinished(controller.get(), task_id);

  // The task ID should be cleared.
  EXPECT_FALSE(HasActingTaskId(controller.get()));
}

IN_PROC_BROWSER_TEST_F(IdentityDialogControllerBrowserTest,
                       ActorTaskHidesUiOnShow) {
  std::unique_ptr<IdentityDialogController> controller =
      std::make_unique<IdentityDialogController>(web_contents_);
  auto mock_view = std::make_unique<MockAccountSelectionView>();
  MockAccountSelectionView* view_ptr = mock_view.get();
  controller->SetAccountSelectionViewForTesting(std::move(mock_view));

  // Expect SetCanShowWidget(false) to be called when we get an active actor
  // task.
  EXPECT_CALL(*view_ptr, SetCanShowWidget(false)).Times(1);

  // Simulate an actor task being active.
  SimulateNewActiveActorTask();

  // Show the accounts dialog.
  std::vector<IdentityRequestAccountPtr> accounts = CreateAccount();
  IdentityProviderDataPtr idp_data = CreateIdentityProviderData(accounts);

  EXPECT_CALL(*view_ptr, Show).WillOnce(testing::Return(true));
  controller->ShowAccountsDialog(
      content::RelyingPartyData(kTopFrameEtldPlusOne,
                                /*iframe_for_display=*/u""),
      {idp_data}, accounts, blink::mojom::RpMode::kActive,
      /*on_selected=*/base::DoNothing(),
      /*on_add_account=*/base::DoNothing(),
      /*dismiss_callback=*/base::DoNothing(),
      /*accounts_displayed_callback=*/base::DoNothing());

  EXPECT_FALSE(controller->DidShowUi());
}
