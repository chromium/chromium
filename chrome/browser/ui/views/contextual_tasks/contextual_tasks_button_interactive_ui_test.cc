// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
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
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_button.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_close_tab_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_tester.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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

  bool IsAimEligible() const override {
    return is_aim_eligible_ &&
           ChromeAimEligibilityService::IsAimAllowedByPolicy(
               &pref_service_.get());
  }

  void SetIsAimEligible(bool eligible) {
    if (is_aim_eligible_ == eligible) {
      return;
    }
    is_aim_eligible_ = eligible;
    OnEligibilityResponseChanged();
  }

  bool IsFuseboxEligible() const override {
    return is_fusebox_eligible_ && IsAimEligible();
  }

  void SetIsFuseboxEligible(bool eligible) {
    if (is_fusebox_eligible_ == eligible) {
      return;
    }
    is_fusebox_eligible_ = eligible;
    OnEligibilityResponseChanged();
  }

  variations::VariationsService* GetVariationsService() const override {
    return nullptr;
  }

 private:
  bool is_aim_eligible_ = true;
  bool is_fusebox_eligible_ = true;
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
        !aim_eligibility_service_->IsAimEligible()) {
      return false;
    }
    return true;
  }

 protected:
  bool CalculateEligibility() const override {
    if (aim_eligibility_service_ &&
        !aim_eligibility_service_->IsAimEligible()) {
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

template <typename T>
  requires std::derived_from<T, InProcessBrowserTest>
class ContextualTasksButtonInteractiveTestMixin : public T {
 public:
  template <typename... Args>
  explicit ContextualTasksButtonInteractiveTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}
  ~ContextualTasksButtonInteractiveTestMixin() override = default;

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
    T::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            this->browser()->GetProfile());
  }

  void TearDownOnMainThread() override {
    identity_test_env_adaptor_.reset();
    T::TearDownOnMainThread();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  PrefService* GetPrefService() {
    return this->browser()->GetProfile()->GetPrefs();
  }

  TestingContextualTasksUiService* GetTestingService() {
    return static_cast<TestingContextualTasksUiService*>(
        contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
            this->browser()->GetProfile()));
  }

  auto SignIntoEligibleAccount() {
    return this->Do([&]() {
      identity_test_env()->MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
      GetTestingService()->GetFakeEligibilityManager()->SetIsEligible(true);
    });
  }

  auto SetMockCookieJarContainsPrimaryAccount(bool contains) {
    return this->Do([&, contains]() {
      GetTestingService()->GetFakeEligibilityManager()->SetIsEligible(contains);
    });
  }

  auto ClearPrimaryAccount() {
    return this->Do([&]() {
#if !BUILDFLAG(IS_CHROMEOS)
      identity_test_env()->ClearPrimaryAccount();
#endif
      GetTestingService()->GetFakeEligibilityManager()->SetIsEligible(false);
    });
  }

  content::WebContents* GetSidePanelWebContents() {
    auto* controller =
        contextual_tasks::ContextualTasksPanelController::From(this->browser());
    return controller->GetActiveWebContents();
  }

  contextual_tasks::ContextualTasksUiService* GetUiService() {
    return contextual_tasks::ContextualTasksUiServiceFactory::
        GetForBrowserContext(this->browser()->GetProfile());
  }

  auto SetIsAimEligible(bool eligible) {
    return this->Do([&, eligible]() {
      auto* service = static_cast<TestingAimEligibilityService*>(
          AimEligibilityServiceFactory::GetForProfile(
              this->browser()->GetProfile()));
      service->SetIsAimEligible(eligible);
      GetTestingService()->GetFakeEligibilityManager()->SetIsEligible(eligible);
    });
  }

  auto SetIsFuseboxEligible(bool eligible) {
    return this->Do([&, eligible]() {
      auto* service = static_cast<TestingAimEligibilityService*>(
          AimEligibilityServiceFactory::GetForProfile(
              this->browser()->GetProfile()));
      service->SetIsFuseboxEligible(eligible);
    });
  }

  GURL GetTestURL() {
    return this->embedded_test_server()->GetURL("example.com", "/title1.html");
  }

  contextual_tasks::ContextualTasksService* GetContextualTasksService() {
    return contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
        this->browser()->GetProfile());
  }

 private:
  base::CallbackListSubscription subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

using ContextualTasksButtonInteractiveTestBase =
    ContextualTasksButtonInteractiveTestMixin<InteractiveBrowserTest>;

template <typename T>
  requires std::derived_from<T, InProcessBrowserTest>
class ContextualTasksEphemeralButtonInteractiveTestMixin
    : public ContextualTasksButtonInteractiveTestMixin<T> {
 public:
  using Base = ContextualTasksButtonInteractiveTestMixin<T>;

  template <typename... Args>
  explicit ContextualTasksEphemeralButtonInteractiveTestMixin(Args&&... args)
      : Base(std::forward<Args>(args)...) {}
  ~ContextualTasksEphemeralButtonInteractiveTestMixin() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasks,
          {{"ContextualTasksExpandButtonOptions", "toolbar-close-button"}}},
         {contextual_tasks::kContextualTasksEphemeralBrandedEntryPoint,
          {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}},
         {contextual_tasks::kContextualTasksHideCloseButtonInVerticalTabs, {}},
         {contextual_tasks::kEnableContextualTasksPinButtonInToolbar, {}}},
        {});
    Base::SetUp();
  }

  void SetUpOnMainThread() override {
    Base::SetUpOnMainThread();
    this->host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(this->embedded_test_server()->Start());
    SidePanelUI::From(this->browser())->DisableAnimationsForTesting();
  }

  auto CreateTaskForTab(int tab_index) {
    return this->Do([&, tab_index] {
      contextual_tasks::ContextualTask task =
          this->GetContextualTasksService()->CreateTask();
      content::WebContents* const web_contents =
          this->browser()->GetTabStripModel()->GetWebContentsAt(tab_index);
      SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents);
      this->GetContextualTasksService()->AssociateTabWithTask(task.GetTaskId(),
                                                              session_id);
      this->GetContextualTasksService()->UpdateThreadForTask(
          task.GetTaskId(), contextual_tasks::ThreadType::kAiMode,
          "test_server_id", std::nullopt, "Test Title");
    });
  }

  auto RemoveTaskFromTab(int tab_index) {
    return this->Do([&, tab_index] {
      content::WebContents* const web_contents =
          this->browser()->GetTabStripModel()->GetWebContentsAt(tab_index);
      SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents);
      std::optional<contextual_tasks::ContextualTask> task =
          this->GetContextualTasksService()->GetContextualTaskForTab(
              session_id);
      if (task.has_value()) {
        this->GetContextualTasksService()->DisassociateTabFromTask(
            task.value().GetTaskId(), session_id);
      }
    });
  }

  auto SimulateOpeningContextualTaskSidePanel() {
    return this->Do([&] {
      contextual_tasks::ContextualTasksPanelController::From(this->browser())
          ->Show();
      content::WebContents* side_panel_contents =
          contextual_tasks::ContextualTasksPanelController::From(
              this->browser())
              ->GetActiveWebContents();
      if (side_panel_contents) {
        contextual_tasks::GetWebUiInterface(side_panel_contents)
            ->SetIsAiPage(true);
      }
    });
  }

  auto SimulateClosingContextualTaskSidePanel() {
    return this->Do([&] {
      contextual_tasks::ContextualTasksPanelController::From(this->browser())
          ->Close();
    });
  }

  auto SimulateNavigateToAiPage() {
    return this->Do([&]() {
      content::WebContents* side_panel_contents =
          contextual_tasks::ContextualTasksPanelController::From(
              this->browser())
              ->GetActiveWebContents();
      contextual_tasks::GetWebUiInterface(side_panel_contents)
          ->SetIsAiPage(true);
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using ContextualTasksEphemeralButtonInteractiveTest =
    ContextualTasksEphemeralButtonInteractiveTestMixin<InteractiveBrowserTest>;

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       ButtonShowsAfterSidePanelWasClosed) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(
          kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0),
      EnsureNotPresent(
          kContextualTasksEphemeralToolbarButtonElementId),
      SimulateOpeningContextualTaskSidePanel(),
      EnsureNotPresent(
          kContextualTasksEphemeralToolbarButtonElementId),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       ButtonVisibilityIsTiedToTab) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(
          kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      SelectTab(kTabStripElementId, 1),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       RapidTabSwitchDuringButtonAnimationDoesNotCrash) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      // Switch tabs back and forth to ensure layer animations and drop shadow
      // tear down cleanly across active tab changes.
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      SelectTab(kTabStripElementId, 1),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      SelectTab(kTabStripElementId, 0),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      SelectTab(kTabStripElementId, 1),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       HideButtonWhenNotAssociatedToTask) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(
          kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      RemoveTaskFromTab(0),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       ButtonVisibilityIsTiedToAimEligibility) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(
          kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      SetIsAimEligible(false),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      SetIsAimEligible(true),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId));
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

// Immersive fullscreen mode is only supported on ChromeOS and macOS.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       CloseButtonHiddenInImmersiveMode) {
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
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       BackgroundUpdatesOnImmersiveModeChange) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateOpeningContextualTaskSidePanel(),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        // Simulate entering immersive mode.
        auto* controller = ImmersiveModeController::From(browser());
        controller->SetEnabled(true);
      }),
      EnsurePresent(kContextualTasksEphemeralToolbarButtonElementId),
      CheckView(kContextualTasksEphemeralToolbarButtonElementId,
                [](ContextualTasksButton* button) {
                  return button->ShouldApplyCircularBackgroundShadow();
                }),
      Do([&]() {
        // Simulate exiting immersive mode.
        auto* controller = ImmersiveModeController::From(browser());
        controller->SetEnabled(false);
      }),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId));
}
#endif

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       HideButtonWhenPinned) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       PinnedButtonVisibilityUpdatesOnEligibilityChange) {
  RunTestSequence(
      SignIntoEligibleAccount(), Do([&]() {
        actions::ActionItem* action_item =
            actions::ActionManager::Get().FindAction(
                kActionSidePanelShowContextualTasks,
                BrowserActions::From(browser())->root_action_item());
        ASSERT_NE(action_item, nullptr);
        EXPECT_TRUE(action_item->GetVisible());
      }),
      ClearPrimaryAccount(), Do([&]() {
        actions::ActionItem* action_item =
            actions::ActionManager::Get().FindAction(
                kActionSidePanelShowContextualTasks,
                BrowserActions::From(browser())->root_action_item());
        ASSERT_NE(action_item, nullptr);
        EXPECT_FALSE(action_item->GetVisible());
      }));
}

using ContextualTasksEphemeralButtonPromoInteractiveTestBase =
    ContextualTasksEphemeralButtonInteractiveTestMixin<
        InteractiveFeaturePromoTest>;

class ContextualTasksEphemeralButtonPromoInteractiveTest
    : public ContextualTasksEphemeralButtonPromoInteractiveTestBase {
 public:
  ContextualTasksEphemeralButtonPromoInteractiveTest()
      : ContextualTasksEphemeralButtonPromoInteractiveTestBase(
            UseDefaultTrackerAllowingPromos(
                {feature_engagement::
                     kIPHContextualTasksEphemeralToolbarButtonFeature})) {}
  ~ContextualTasksEphemeralButtonPromoInteractiveTest() override = default;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonPromoInteractiveTest,
                       ShowsPromoWhenEphemeralButtonAppears) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      WaitForPromo(
          feature_engagement::kIPHContextualTasksEphemeralToolbarButtonFeature),
      PressButton(kContextualTasksEphemeralToolbarButtonElementId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

class ContextualTasksEphemeralBrandedButtonInteractiveTest
    : public ContextualTasksEphemeralButtonInteractiveTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasks, {}},
         {contextual_tasks::kContextualTasksEphemeralBrandedEntryPoint,
          {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}}},
        {});
    InteractiveBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       ButtonHidesOnContextualTasksPage) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(
          kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      NavigateWebContents(kFirstTab, GURL(chrome::kChromeUIContextualTasksURL)),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       LogsEphemeralMetricsOnToolbarButtonShown) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.Shown"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.Shown", true, 1);
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       LogsEphemeralMetricsOnToolbarButtonPress) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      PressButton(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "OpenSidePanel"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction.OpenSidePanel",
            true, 1);
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "ContextualTasks.PermanentToolbarButton.UserAction."
                         "OpenSidePanel"));
      }),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       LogsPermanentMetricsOnToolbarButtonPress) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0), Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.PermanentToolbarButton.UserAction."
                         "OpenSidePanel"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.PermanentToolbarButton.UserAction.OpenSidePanel",
            true, 1);
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "OpenSidePanel"));
      }),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.PermanentToolbarButton.UserAction."
                         "CloseSidePanel"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.PermanentToolbarButton.UserAction.CloseSidePanel",
            true, 1);
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "CloseSidePanel"));
      }));
}

class ContextualTasksEphemeralButtonCobrowseDisabledInteractiveTest
    : public ContextualTasksButtonInteractiveTestBase {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasksSidePanel, {}},
         {contextual_tasks::kContextualTasksEphemeralBrandedEntryPoint,
          {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}},
         {contextual_tasks::kEnableContextualTasksPinButtonInToolbar, {}}},
        {contextual_tasks::kContextualTasks});
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ContextualTasksButtonInteractiveTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    SidePanelUI::From(browser())->DisableAnimationsForTesting();
  }

  auto CreateTaskForTab(int tab_index) {
    return Do([this, tab_index] {
      tabs::TabInterface* tab =
          browser()->GetTabStripModel()->GetTabAtIndex(tab_index);
      contextual_tasks::ContextualTask task =
          GetContextualTasksService()->CreateTask();
      GetContextualTasksService()->AssociateTabWithTask(
          task.GetTaskId(),
          sessions::SessionTabHelper::IdForTab(tab->GetContents()));
      GetContextualTasksService()->UpdateThreadForTask(
          task.GetTaskId(), contextual_tasks::ThreadType::kAiMode,
          "test_server_id", std::nullopt, "Test Title");
    });
  }

  auto SimulateOpeningContextualTaskSidePanel() {
    return Do([&] {
      contextual_tasks::ContextualTasksPanelController::From(browser())->Show();
      content::WebContents* side_panel_contents =
          contextual_tasks::ContextualTasksPanelController::From(browser())
              ->GetActiveWebContents();
      if (side_panel_contents) {
        contextual_tasks::GetWebUiInterface(side_panel_contents)
            ->SetIsAiPage(true);
      }
    });
  }

  auto SimulateClosingContextualTaskSidePanel() {
    return Do([&] {
      contextual_tasks::ContextualTasksPanelController::From(browser())
          ->Close();
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonCobrowseDisabledInteractiveTest,
    ShowsEphemeralButtonWhenCobrowseDisabled) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateOpeningContextualTaskSidePanel(),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonCobrowseDisabledInteractiveTest,
    PermanentPinButtonOpensNewZeroStateWhileEphemeralResumes) {
  base::Uuid initial_task_id;
  base::Uuid resumed_task_id;
  base::Uuid new_task_id;

  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      // Create initial task and open side panel.
      CreateTaskForTab(0), Do([&]() {
        tabs::TabInterface* tab =
            browser()->GetTabStripModel()->GetTabAtIndex(0);
        initial_task_id =
            GetContextualTasksService()
                ->GetContextualTaskForTab(
                    sessions::SessionTabHelper::IdForTab(tab->GetContents()))
                ->GetTaskId();
      }),
      SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      // Pressing the ephemeral button resumes the original task.
      PressButton(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        tabs::TabInterface* tab =
            browser()->GetTabStripModel()->GetTabAtIndex(0);
        resumed_task_id =
            GetContextualTasksService()
                ->GetContextualTaskForTab(
                    sessions::SessionTabHelper::IdForTab(tab->GetContents()))
                ->GetTaskId();
        EXPECT_EQ(initial_task_id, resumed_task_id);
      }),
      SimulateClosingContextualTaskSidePanel(),
      // Pin to toolbar and press permanent button.
      Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      Do([&]() {
        tabs::TabInterface* tab =
            browser()->GetTabStripModel()->GetTabAtIndex(0);
        new_task_id =
            GetContextualTasksService()
                ->GetContextualTaskForTab(
                    sessions::SessionTabHelper::IdForTab(tab->GetContents()))
                ->GetTaskId();
        EXPECT_NE(initial_task_id, new_task_id);
      }));
}

