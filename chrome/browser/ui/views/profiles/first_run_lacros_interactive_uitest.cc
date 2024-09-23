// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/browser/ui/startup/first_run_test_util.h"
#include "chrome/browser/ui/views/profiles/profile_picker_interactive_uitest_base.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/view_class_properties.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kProfilePickerViewId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kButtonEnabled);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kButtonDisabled);

using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;
const DeepQuery kProceedButton{"intro-app", "#proceedButton"};
const DeepQuery kOptInSyncButton{"sync-confirmation-app", "#confirmButton"};

}  // namespace

class FirstRunLacrosInteractiveUiTest
    : public InteractiveBrowserTestT<FirstRunServiceBrowserTestBase>,
      public WithProfilePickerInteractiveUiTestHelpers {
 public:
  FirstRunLacrosInteractiveUiTest() {
    scoped_chrome_build_override_ = std::make_unique<base::AutoReset<bool>>(
        SearchEngineChoiceDialogServiceFactory::
            ScopedChromeBuildOverrideForTesting(
                /*force_chrome_build=*/true));
  }

  // FirstRunInteractiveUiTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FirstRunServiceBrowserTestBase::SetUpCommandLine(command_line);

    // Change the country to Belgium because the search engine choice screen is
    // only displayed for EEA countries.
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry, "BE");

    command_line->AppendSwitch(
        switches::kIgnoreNoFirstRunForSearchEngineChoiceScreen);
  }

  void TearDownOnMainThread() override {
    FirstRunServiceBrowserTestBase::TearDownOnMainThread();
    identity_test_env_adaptor_.reset();
  }

  void SetUpOnMainThread() override {
    FirstRunServiceBrowserTestBase::SetUpOnMainThread();
    SearchEngineChoiceDialogService::SetDialogDisabledForTests(
        /*dialog_disabled=*/false);

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    identity_test_env_adaptor_->identity_test_env()
        ->SetRefreshTokenForPrimaryAccount();
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&FirstRunLacrosInteractiveUiTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OpenFirstRun(base::OnceCallback<void(bool)> first_run_exited_callback =
                        base::OnceCallback<void(bool)>()) {
    ASSERT_TRUE(fre_service()->ShouldOpenFirstRun());

    fre_service()->OpenFirstRunIfNeeded(FirstRunService::EntryPoint::kOther,
                                        std::move(first_run_exited_callback));

    WaitForPickerWidgetCreated();
    view()->SetProperty(views::kElementIdentifierKey, kProfilePickerViewId);
  }

  auto PressJsButton(const ui::ElementIdentifier web_contents_id,
                     const DeepQuery& button_query) {
    // This can close/navigate the current page, so don't wait for success.
    return ExecuteJsAt(web_contents_id, button_query, "(btn) => btn.click()",
                       ExecuteJsMode::kFireAndForget);
  }

  auto WaitForButtonEnabled(const ui::ElementIdentifier web_contents_id,
                            const DeepQuery& button_query) {
    StateChange button_enabled;
    button_enabled.event = kButtonEnabled;
    button_enabled.where = button_query;
    button_enabled.type = StateChange::Type::kExistsAndConditionTrue;
    button_enabled.test_function = "(btn) => !btn.disabled";
    return WaitForStateChange(web_contents_id, button_enabled);
  }

  auto WaitForButtonDisabled(const ui::ElementIdentifier web_contents_id,
                             const DeepQuery& button_query) {
    StateChange button_disabled;
    button_disabled.event = kButtonDisabled;
    button_disabled.where = button_query;
    button_disabled.type = StateChange::Type::kExistsAndConditionTrue;
    button_disabled.test_function = "(btn) => btn.disabled";
    return WaitForStateChange(web_contents_id, button_disabled);
  }

  auto CompleteSearchEngineChoiceStep() {
    const DeepQuery kFirstSearchEngine = {"search-engine-choice-app",
                                          "cr-radio-button"};
    const DeepQuery kSearchEngineChoiceActionButton = {
        "search-engine-choice-app", "#actionButton"};
    return Steps(
        WaitForWebContentsNavigation(
            kWebContentsId, GURL(chrome::kChromeUISearchEngineChoiceURL)),
        // Click on "More" to scroll to the bottom of the search engine list.
        PressJsButton(kWebContentsId, kSearchEngineChoiceActionButton),
        // The button should become disabled because we didn't make a choice.
        WaitForButtonDisabled(kWebContentsId, kSearchEngineChoiceActionButton),
        PressJsButton(kWebContentsId, kFirstSearchEngine),
        WaitForButtonEnabled(kWebContentsId, kSearchEngineChoiceActionButton),
        PressJsButton(kWebContentsId, kSearchEngineChoiceActionButton));
  }

  const base::HistogramTester& HistogramTester() const {
    return histogram_tester_;
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<base::AutoReset<bool>> scoped_chrome_build_override_;
};

IN_PROC_BROWSER_TEST_F(FirstRunLacrosInteractiveUiTest,
                       AcceptSyncAndFinishFlow) {
  base::test::TestFuture<bool> proceed_future;
  ASSERT_TRUE(IsProfileNameDefault());
  OpenFirstRun(proceed_future.GetCallback());
  GURL sync_page_url = AppendSyncConfirmationQueryParams(
      GURL("chrome://sync-confirmation/"), SyncConfirmationStyle::kWindow,
      /*is_sync_promo=*/true);

  RunTestSequenceInContext(
      views::ElementTrackerViews::GetContextForView(view()),

      // Wait for the profile picker to show the intro.
      WaitForShow(kProfilePickerViewId),
      InstrumentNonTabWebView(kWebContentsId, web_view()),
      WaitForWebContentsReady(kWebContentsId, GURL(chrome::kChromeUIIntroURL)),

      // Advance to the sync confirmation page.
      PressJsButton(kWebContentsId, kProceedButton),
      WaitForWebContentsNavigation(kWebContentsId, sync_page_url),

      // Accept sync.
      EnsurePresent(kWebContentsId, kOptInSyncButton),
      PressJsButton(kWebContentsId, kOptInSyncButton)
          .SetMustRemainVisible(false),

      CompleteSearchEngineChoiceStep());

  WaitForPickerClosed();

  HistogramTester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kFreDefaultWasSet, 1);
  PrefService* pref_service = browser()->profile()->GetPrefs();
  EXPECT_TRUE(pref_service->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));

  EXPECT_TRUE(GetFirstRunFinishedPrefValue());
  EXPECT_FALSE(fre_service()->ShouldOpenFirstRun());
  EXPECT_FALSE(IsUsingDefaultProfileName());
}
