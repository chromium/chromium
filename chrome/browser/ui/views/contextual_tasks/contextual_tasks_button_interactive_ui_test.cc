// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_button.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_close_tab_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_tester.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);

class TestingAimEligibilityService : public ChromeAimEligibilityService {
 public:
  explicit TestingAimEligibilityService(
      PrefService& pref_service,
      TemplateURLService* template_url_service)
      : ChromeAimEligibilityService(pref_service,
                                    template_url_service,
                                    /*url_loader_factory=*/nullptr,
                                    /*identity_manager=*/nullptr,
                                    /*configuration=*/{}),
        pref_service_(pref_service) {}

  ~TestingAimEligibilityService() override = default;

  bool IsAimEligible() const override { return true; }
  bool IsCobrowseEligible() const override {
    return is_cobrowse_eligible_ &&
           ChromeAimEligibilityService::IsAimAllowedByPolicy(
               &pref_service_.get());
  }

  void SetIsCobrowseEligible(bool eligible) {
    if (is_cobrowse_eligible_ == eligible) {
      return;
    }
    is_cobrowse_eligible_ = eligible;
    OnEligibilityResponseChanged();
  }

  variations::VariationsService* GetVariationsService() const override {
    return nullptr;
  }

 private:
  bool is_cobrowse_eligible_ = true;
  const base::raw_ref<PrefService> pref_service_;
};

class FakeContextualTasksEligibilityManager
    : public contextual_tasks::ContextualTasksEligibilityManager {
 public:
  FakeContextualTasksEligibilityManager(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service)
      : ContextualTasksEligibilityManager(pref_service,
                                          identity_manager,
                                          aim_eligibility_service) {
    MaybeNotifyEligibilityChanged();
  }
  ~FakeContextualTasksEligibilityManager() override = default;

  void SetIsEligible(bool eligible) {
    if (mock_identity_eligible_ == eligible) {
      return;
    }
    mock_identity_eligible_ = eligible;
    MaybeNotifyEligibilityChanged();
  }

  bool IsEligibleWithoutIdentity() const override {
    if (aim_eligibility_service_ &&
        !aim_eligibility_service_->IsCobrowseEligible()) {
      return false;
    }
    return true;
  }

 protected:
  bool CalculateEligibility() const override {
    if (aim_eligibility_service_ &&
        !aim_eligibility_service_->IsCobrowseEligible()) {
      return false;
    }
    return mock_identity_eligible_;
  }

 private:
  bool mock_identity_eligible_ = false;
};

class TestingContextualTasksUiService
    : public contextual_tasks::ContextualTasksUiService {
 public:
  TestingContextualTasksUiService(
      Profile* profile,
      contextual_tasks::ContextualTasksService* contextual_tasks_service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service)
      : ContextualTasksUiService(
            profile,
            std::make_unique<
                contextual_tasks::MockContextualTasksUiServiceDelegate>(),
            contextual_tasks_service,
            identity_manager,
            aim_eligibility_service,
            std::make_unique<FakeContextualTasksEligibilityManager>(
                profile->GetPrefs(),
                identity_manager,
                aim_eligibility_service),
            /*cookie_synchronizer=*/nullptr) {}
  ~TestingContextualTasksUiService() override = default;

  FakeContextualTasksEligibilityManager* GetFakeEligibilityManager() {
    return static_cast<FakeContextualTasksEligibilityManager*>(
        GetEligibilityManager());
  }
};
}  // namespace

class ContextualTasksButtonInteractiveTestBase : public InteractiveBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  IdentityTestEnvironmentProfileAdaptor::
                      SetIdentityTestEnvironmentFactoriesOnBrowserContext(
                          context);

                  AimEligibilityServiceFactory::GetInstance()
                      ->SetTestingFactory(
                          context,
                          base::BindLambdaForTesting([](content::BrowserContext*
                                                            context) {
                            Profile* const profile =
                                Profile::FromBrowserContext(context);
                            return static_cast<std::unique_ptr<KeyedService>>(
                                std::make_unique<TestingAimEligibilityService>(
                                    *profile->GetPrefs(),
                                    TemplateURLServiceFactory::GetForProfile(
                                        profile)));
                          }));

                  contextual_tasks::ContextualTasksUiServiceFactory::
                      GetInstance()
                          ->SetTestingFactory(
                              context,
                              base::BindLambdaForTesting(
                                  [](content::BrowserContext* context) {
                                    Profile* profile =
                                        Profile::FromBrowserContext(context);
                                    return static_cast<
                                        std::unique_ptr<KeyedService>>(
                                        std::make_unique<
                                            TestingContextualTasksUiService>(
                                            profile,
                                            contextual_tasks::
                                                ContextualTasksServiceFactory::
                                                    GetForProfile(profile),
                                            IdentityManagerFactory::
                                                GetForProfile(profile),
                                            AimEligibilityServiceFactory::
                                                GetForProfile(profile)));
                                  }));
                }));
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
  }

  void TearDownOnMainThread() override {
    identity_test_env_adaptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  PrefService* GetPrefService() { return browser()->profile()->GetPrefs(); }

  TestingContextualTasksUiService* GetTestingService() {
    return static_cast<TestingContextualTasksUiService*>(
        contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
            browser()->profile()));
  }

  auto SignIntoEligibleAccount() {
    return Do([&]() {
      identity_test_env()->MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
      GetTestingService()->GetFakeEligibilityManager()->SetIsEligible(true);
    });
  }

  auto SetMockCookieJarContainsPrimaryAccount(bool contains) {
    return Do([&, contains]() {
      GetTestingService()->GetFakeEligibilityManager()->SetIsEligible(contains);
    });
  }

  auto ClearPrimaryAccount() {
    return Do([&]() {
      identity_test_env()->ClearPrimaryAccount();
      GetTestingService()->GetFakeEligibilityManager()->SetIsEligible(false);
    });
  }

  content::WebContents* GetSidePanelWebContents() {
    auto* controller =
        contextual_tasks::ContextualTasksPanelController::From(browser());
    return controller->GetActiveWebContents();
  }

  contextual_tasks::ContextualTasksUiService* GetUiService() {
    return contextual_tasks::ContextualTasksUiServiceFactory::
        GetForBrowserContext(browser()->profile());
  }

  auto SetIsCobrowseEligible(bool eligible) {
    return Do([&, eligible]() {
      auto* service = static_cast<TestingAimEligibilityService*>(
          AimEligibilityServiceFactory::GetForProfile(browser()->profile()));
      service->SetIsCobrowseEligible(eligible);
      GetTestingService()->GetFakeEligibilityManager()->SetIsEligible(eligible);
    });
  }

 private:
  base::CallbackListSubscription subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};


class ContextualTasksEphemeralButtonInteractiveTest
    : public ContextualTasksButtonInteractiveTestBase {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasks,
          {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"},
           {"ContextualTasksExpandButtonOptions", "toolbar-close-button"}}},
         {contextual_tasks::kContextualTasksHideCloseButtonInVerticalTabs, {}},
         {tabs::kVerticalTabs, {}}},
        {});
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ContextualTasksButtonInteractiveTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    browser()
        ->browser_window_features()
        ->side_panel_ui()
        ->DisableAnimationsForTesting();
  }

  GURL GetTestURL() {
    return embedded_test_server()->GetURL("example.com", "/title1.html");
  }

  contextual_tasks::ContextualTasksService* GetContextualTasksService() {
    return contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
        browser()->profile());
  }

  auto CreateTaskForTab(int tab_index) {
    return Do([&, tab_index] {
      contextual_tasks::ContextualTask task =
          GetContextualTasksService()->CreateTask();
      content::WebContents* const web_contents =
          browser()->tab_strip_model()->GetWebContentsAt(tab_index);
      SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents);
      GetContextualTasksService()->AssociateTabWithTask(task.GetTaskId(),
                                                        session_id);
    });
  }

  auto RemoveTaskFromTab(int tab_index) {
    return Do([&, tab_index] {
      content::WebContents* const web_contents =
          browser()->tab_strip_model()->GetWebContentsAt(tab_index);
      SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents);
      std::optional<contextual_tasks::ContextualTask> task =
          GetContextualTasksService()->GetContextualTaskForTab(session_id);
      if (task.has_value()) {
        GetContextualTasksService()->DisassociateTabFromTask(
            task.value().GetTaskId(), session_id);
      }
    });
  }

  // Simulate opening the contextual task side panel as if you clicked on a link
  // in a contextual task.
  auto SimulateOpeningContextualTaskSidePanel() {
    return Do([&] {
      contextual_tasks::ContextualTasksPanelController::From(browser())->Show();
    });
  }

  // Simulate closing the contextual task side panel as if you clicked on the
  // close button in the contextual task side panel.
  auto SimulateClosingContextualTaskSidePanel() {
    return Do([&] {
      contextual_tasks::ContextualTasksPanelController::From(browser())
          ->Close();
    });
  }

  auto SimulateNavigateToAiPage() {
    return Do([&]() {
      content::WebContents* side_panel_contents =
          contextual_tasks::ContextualTasksPanelController::From(browser())
              ->GetActiveWebContents();
      contextual_tasks::GetWebUiInterface(side_panel_contents)
          ->SetIsAiPage(true);
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       ButtonShowsAfterSidePanelWasClosed) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      CreateTaskForTab(0),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      SimulateOpeningContextualTaskSidePanel(),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       ButtonVisibilityIsTiedToTab) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton),
      SelectTab(kTabStripElementId, 1),
      WaitForHide(ContextualTasksButton::kContextualTasksToolbarButton));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       HideButtonWhenNotAssociatedToTask) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton),
      RemoveTaskFromTab(0),
      WaitForHide(ContextualTasksButton::kContextualTasksToolbarButton));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       ButtonVisibilityIsTiedToAimCobrowseEligibility) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton),
      SetIsCobrowseEligible(false),
      WaitForHide(ContextualTasksButton::kContextualTasksToolbarButton),
      SetIsCobrowseEligible(true),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       ButtonVisibilityIsPreservedAsSidePanelToggles) {
  if ((true)) {
    GTEST_SKIP() << "Branded variant button visibility behavior differs.";
  }

  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 1),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      CreateTaskForTab(0), SelectTab(kTabStripElementId, 0),
      SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton),
      PressButton(ContextualTasksButton::kContextualTasksToolbarButton),
      WaitForShow(kSidePanelElementId),
      EnsurePresent(ContextualTasksButton::kContextualTasksToolbarButton),
      EnsureNotPresent(
          ContextualTasksCloseTabButton::kContextualTasksCloseTabButton),
      SimulateNavigateToAiPage(),
      EnsurePresent(
          ContextualTasksCloseTabButton::kContextualTasksCloseTabButton),
      PressButton(ContextualTasksButton::kContextualTasksToolbarButton),
      WaitForHide(kSidePanelElementId),
      EnsurePresent(ContextualTasksButton::kContextualTasksToolbarButton),
      EnsureNotPresent(
          ContextualTasksCloseTabButton::kContextualTasksCloseTabButton));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       CloseButtonHiddenInVerticalTabs) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0), CreateTaskForTab(0),
      SimulateOpeningContextualTaskSidePanel(), SimulateNavigateToAiPage(),
      // Ensure close button is visible in horizontal mode.
      EnsurePresent(
          ContextualTasksCloseTabButton::kContextualTasksCloseTabButton),
      // Switch to vertical tabs.
      Do([&]() {
        auto* controller =
            tabs::VerticalTabStripStateController::From(browser());
        CHECK(controller);
        controller->SetVerticalTabsEnabled(true);
      }),
      // Verify close button is hidden.
      EnsureNotPresent(
          ContextualTasksCloseTabButton::kContextualTasksCloseTabButton));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       CloseButtonHiddenInImmersiveMode) {
#if !(BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC))
  GTEST_SKIP() << "Immersive mode not supported on this platform.";
#else
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0), CreateTaskForTab(0),
      SimulateOpeningContextualTaskSidePanel(), SimulateNavigateToAiPage(),
      // Ensure close button is visible in non-immersive mode.
      EnsurePresent(
          ContextualTasksCloseTabButton::kContextualTasksCloseTabButton),
      // Switch to immersive mode.
      Do([&]() {
        auto* controller = ImmersiveModeController::From(browser());
        controller->SetEnabled(true);
      }),
      // Verify close button is hidden.
      EnsureNotPresent(
          ContextualTasksCloseTabButton::kContextualTasksCloseTabButton));
#endif
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       BackgroundUpdatesOnImmersiveModeChange) {
#if !BUILDFLAG(IS_CHROMEOS) || !BUILDFLAG(IS_MAC)
  GTEST_SKIP() << "Immersive mode not supported on this platform.";
#else
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      CreateTaskForTab(0),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      SimulateOpeningContextualTaskSidePanel(),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton),
      Do([&]() {
        // Simulate entering immersive mode.
        auto* controller = ImmersiveModeController::From(browser());
        controller->SetEnabled(true);
      }),
      EnsurePresent(ContextualTasksButton::kContextualTasksToolbarButton),
      CheckView(ContextualTasksButton::kContextualTasksToolbarButton,
                [](ContextualTasksButton* button) {
                  return button->GetBackground() != nullptr;
                }),
      Do([&]() {
        // Simulate exiting immersive mode.
        auto* controller = ImmersiveModeController::From(browser());
        controller->SetEnabled(false);
      }),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton));
#endif
}


class ContextualTasksEphemeralBrandedButtonInteractiveTest
    : public ContextualTasksEphemeralButtonInteractiveTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasks,
          {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}}},
        {});
    InteractiveBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralBrandedButtonInteractiveTest,
                       ButtonHidesOnContextualTasksPage) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton),
      NavigateWebContents(kFirstTab, GURL(chrome::kChromeUIContextualTasksURL)),
      WaitForHide(ContextualTasksButton::kContextualTasksToolbarButton));
}
