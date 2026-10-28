// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
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
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_button.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_close_tab_button.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_controller.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_menu_model.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_tester.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_solid_color.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_MAC)
#include "ui/views/controls/menu/menu_cocoa_watcher_mac.h"
#endif

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

  bool IsSidePanelAvailable() const override {
    if (!contextual_tasks::IsContextualTasksUIEnabled()) {
      return false;
    }

    if (base::FeatureList::IsEnabled(
            contextual_tasks::kContextualTasksForceEntryPointEligibility)) {
      return true;
    }

    if (mock_identity_eligible_) {
      return true;
    }

    return lens::features::IsLensSidePanelUnificationAllowSignedOut();
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
    // These tests expect the contextual tasks button to start docked on the
    // leading edge of the toolbar, so align the contextual tasks side panel to
    // the left instead of relying on its default alignment.
    ScopedDictPrefUpdate(this->browser()->GetProfile()->GetPrefs(),
                         prefs::kSidePanelAlignmentOverrides)
        ->Set(SidePanelEntryIdToString(SidePanelEntryId::kContextualTasks),
              false);
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
      if (!identity_test_env()->identity_manager()->HasPrimaryAccount(
              signin::ConsentLevel::kSignin)) {
        identity_test_env()->MakePrimaryAccountAvailable(
            "primary@example.com", signin::ConsentLevel::kSignin);
      }
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
    scoped_feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                       GetDisabledFeatures());
    Base::SetUp();
  }

  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {
        {contextual_tasks::kContextualTasks,
         {{"ContextualTasksExpandButtonOptions", "toolbar-close-button"}}},
        {contextual_tasks::kContextualTasksEphemeralBrandedEntryPoint,
         {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}},
        {contextual_tasks::kContextualTasksHideCloseButtonInVerticalTabs, {}},
        {contextual_tasks::kEnableContextualTasksPinButtonInToolbar, {}},
        {contextual_tasks::kContextualTasksEphemeralButtonContextMenu, {}}};
  }

  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    return {};
  }

  // Side panel animations hide the timing windows that rapid clicks expose, so
  // tests that exercise those windows have to keep animations on.
  virtual bool ShouldDisableSidePanelAnimations() const { return true; }

  void SetUpOnMainThread() override {
    Base::SetUpOnMainThread();
#if BUILDFLAG(IS_MAC)
    views::MenuCocoaWatcherMac::SetNotificationFilterForTesting(
        views::MacNotificationFilter::IgnoreAllNotifications);
#endif
    this->host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(this->embedded_test_server()->Start());
    if (ShouldDisableSidePanelAnimations()) {
      SidePanelUI::From(this->browser())->DisableAnimationsForTesting();
    }
  }

  void TearDownOnMainThread() override {
#if BUILDFLAG(IS_MAC)
    views::MenuCocoaWatcherMac::SetNotificationFilterForTesting(
        views::MacNotificationFilter::DontIgnoreNotifications);
#endif
    Base::TearDownOnMainThread();
  }

  auto ShowToolbarButtonContextMenu(ui::ElementIdentifier button_id) {
    return this->WithView(button_id, [](views::View* view) {
      if (auto* container = views::AsViewClass<PinnedToolbarActionsContainer>(
              view->parent())) {
        container->GetAnimatingLayoutManager()->ResetLayout();
      }
      view->GetWidget()->LayoutRootViewIfNecessary();
      view->ShowContextMenu(view->GetBoundsInScreen().CenterPoint(),
                            ui::mojom::MenuSourceType::kMouse);
    });
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
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateOpeningContextualTaskSidePanel(),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       RightClickRemoveHidesEphemeralButtonUntilNextAdded) {
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
      // Open the ephemeral toolbar button's context menu and select
      // Unpin/Remove.
      ShowToolbarButtonContextMenu(
          kContextualTasksEphemeralToolbarButtonElementId),
      SelectMenuItem(
          ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "OpenContextMenu"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction.OpenContextMenu",
            true, 1);
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "OpenContextMenu.CloseSidePanelOptionVisible"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction."
            "OpenContextMenu.CloseSidePanelOptionVisible",
            true, 1);
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "OpenContextMenu.CloseSidePanelOptionHidden"));
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.UserAction."
            "OpenContextMenu.CloseSidePanelOptionHidden",
            0);
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "RemoveByContextMenu"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction."
            "RemoveByContextMenu",
            true, 1);
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration", 1);
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration."
            "RemovedByContextMenu",
            1);
      }),
      // Switching away and back keeps the ephemeral button removed.
      SelectTab(kTabStripElementId, 1), SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      // Starting a new task and opening/closing the side panel again adds the
      // next ephemeral button.
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       ButtonShowsWhenSignedOutAndFuseboxIneligible) {
  RunTestSequence(
      SetIsFuseboxEligible(false), InstrumentTab(kFirstTab),
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

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       ButtonVisibilityIsTiedToTab) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      SelectTab(kTabStripElementId, 1),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       ButtonDoesNotShowOnNewTabOpenedFromClosedPanelTab) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      Do([&]() {
        chrome::AddSelectedTabWithURL(browser(), GetTestURL(),
                                      ui::PAGE_TRANSITION_LINK);
      }),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      SelectTab(kTabStripElementId, 0),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId));
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
  base::HistogramTester histogram_tester;

  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      RemoveTaskFromTab(0),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration", 1);
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration."
            "TaskDisassociated",
            1);
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       ButtonVisibilityNotTiedToAimEligibility) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      SetIsAimEligible(false),
      EnsurePresent(kContextualTasksEphemeralToolbarButtonElementId));
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
      CheckViewProperty(kContextualTasksEphemeralToolbarButtonElementId,
                        &ContextualTasksButton::GetShape,
                        contextual_tasks::EphemeralButtonShape::kCircle),
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
                       ButtonDoesNotShowWhenActiveTaskInZeroState) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      // Create a task without a thread (zero state) for tab 0.
      Do([&] {
        contextual_tasks::ContextualTask task =
            GetContextualTasksService()->CreateTask();
        content::WebContents* const web_contents =
            browser()->GetTabStripModel()->GetWebContentsAt(0);
        SessionID session_id =
            sessions::SessionTabHelper::IdForTab(web_contents);
        GetContextualTasksService()->AssociateTabWithTask(task.GetTaskId(),
                                                          session_id);
      }),
      SimulateOpeningContextualTaskSidePanel(),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateClosingContextualTaskSidePanel(),
      // Button should not show when the side panel is closed if the active
      // task is still in zero state (not showing results).
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId));
}

class ContextualTasksEphemeralButtonFeatureEnabledInteractiveTest
    : public ContextualTasksEphemeralButtonInteractiveTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        ContextualTasksEphemeralButtonInteractiveTest::GetEnabledFeatures();
    enabled_features.push_back(
        {contextual_tasks::kEphemeralPinningVisibleWhenPermanentlyPinned, {}});
    return enabled_features;
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonFeatureEnabledInteractiveTest,
    ButtonDoesNotHideWhenPinned) {
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
      EnsurePresent(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonFeatureEnabledInteractiveTest,
    LogsEphemeralMetricsOnToolbarButtonPressWhenPinned) {
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
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      EnsurePresent(kContextualTasksEphemeralToolbarButtonElementId),
      PressButton(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "OpenSidePanel"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction.OpenSidePanel",
            true, 1);
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration", 1);
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration.Clicked", 1);
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "ContextualTasks.PermanentToolbarButton.UserAction."
                         "OpenSidePanel"));
      }));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonFeatureEnabledInteractiveTest,
    ButtonDoesNotShowWhenClosingZeroStateFromPinnedButton) {
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
      EnsurePresent(kContextualTasksEphemeralToolbarButtonElementId),
      // Pressing the pinned button opens the panel in zero state.
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      // Closing the side panel leaves tab 0 associated with the zero-state
      // task. The ephemeral button should not show because no results were
      // loaded in zero state.
      SimulateClosingContextualTaskSidePanel(),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId));
}

// Keeps the side panel's open/close animations running so that a rapid double
// tap of the pinned button leaves a show in flight while the panel closes.
class ContextualTasksEphemeralButtonAnimatedPinnedInteractiveTest
    : public ContextualTasksEphemeralButtonFeatureEnabledInteractiveTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        ContextualTasksEphemeralButtonFeatureEnabledInteractiveTest::
            GetEnabledFeatures();
    enabled_features.push_back(
        {contextual_tasks::kContextualTasksPinningV2, {}});
    return enabled_features;
  }

  bool ShouldDisableSidePanelAnimations() const override { return false; }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonAnimatedPinnedInteractiveTest,
    ButtonStaysVisibleAfterDoubleTappingPinnedButton) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0), CreateTaskForTab(0), Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      // Double tap: the first press opens the panel, the second closes it
      // again before the open has settled.
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      // The button takes over as soon as the panel starts leaving.
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      // ...and it has to survive the cleanup that lands afterwards. The
      // zero-state task has no thread yet, so it is torn down once the panel
      // stops holding it, which used to retire the button along with it.
      WaitForEvent(kSidePanelElementId,
                   SidePanel::kCloseAnimationCompletedEvent),
      EnsurePresent(kContextualTasksEphemeralToolbarButtonElementId));
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
      SetIsFuseboxEligible(false), Do([&]() {
        actions::ActionItem* action_item =
            actions::ActionManager::Get().FindAction(
                kActionSidePanelShowContextualTasks,
                BrowserActions::From(browser())->root_action_item());
        ASSERT_NE(action_item, nullptr);
        EXPECT_FALSE(action_item->GetVisible());
      }),
      SetIsFuseboxEligible(true), Do([&]() {
        actions::ActionItem* action_item =
            actions::ActionManager::Get().FindAction(
                kActionSidePanelShowContextualTasks,
                BrowserActions::From(browser())->root_action_item());
        ASSERT_NE(action_item, nullptr);
        EXPECT_TRUE(action_item->GetVisible());
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       PressingEphemeralButtonFocusesSidePanel) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0), CreateTaskForTab(0),
      SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      PressButton(kContextualTasksEphemeralToolbarButtonElementId),
      WaitForShow(kSidePanelElementId), Check([&]() {
        auto* controller =
            contextual_tasks::ContextualTasksPanelController::From(browser());
        return controller && controller->IsPanelOpenForContextualTask();
      }),
      Check([&]() {
        content::WebContents* wc = GetSidePanelWebContents();
        return wc && wc->GetRenderWidgetHostView() &&
               wc->GetRenderWidgetHostView()->HasFocus();
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       PressingPinnedButtonFocusesSidePanel) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0), Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WaitForShow(kSidePanelElementId), Check([&]() {
        auto* controller =
            contextual_tasks::ContextualTasksPanelController::From(browser());
        return controller && controller->IsPanelOpenForContextualTask();
      }),
      Check([&]() {
        content::WebContents* wc = GetSidePanelWebContents();
        return wc && wc->GetRenderWidgetHostView() &&
               wc->GetRenderWidgetHostView()->HasFocus();
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
      CheckView(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
          [](user_education::HelpBubbleView* bubble) {
            return bubble->arrow();
          },
          views::BubbleBorder::TOP_LEFT),
      PressButton(kContextualTasksEphemeralToolbarButtonElementId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

class ContextualTasksRightEphemeralButtonPromoInteractiveTest
    : public ContextualTasksEphemeralButtonPromoInteractiveTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {
        {contextual_tasks::kContextualTasks,
         {{"ContextualTasksExpandButtonOptions", "toolbar-close-button"}}},
        {contextual_tasks::kContextualTasksEphemeralBrandedEntryPoint,
         {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"},
          {contextual_tasks::kEnableCircularEphemeralButtonNextToBatterySaver
               .name,
           "true"}}},
        {contextual_tasks::kContextualTasksHideCloseButtonInVerticalTabs, {}},
        {contextual_tasks::kEnableContextualTasksPinButtonInToolbar, {}},
        {contextual_tasks::kContextualTasksEphemeralButtonContextMenu, {}}};
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksRightEphemeralButtonPromoInteractiveTest,
    ShowsPromoWithTopRightArrowWhenEphemeralButtonIsOnRight) {
  RunTestSequence(
      SignIntoEligibleAccount(), Do([this]() {
        ScopedDictPrefUpdate(browser()->GetProfile()->GetPrefs(),
                             prefs::kSidePanelAlignmentOverrides)
            ->Set(SidePanelEntryIdToString(SidePanelEntryId::kContextualTasks),
                  true);
      }),
      InstrumentTab(kFirstTab), AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      WaitForPromo(
          feature_engagement::kIPHContextualTasksEphemeralToolbarButtonFeature),
      CheckView(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
          [](user_education::HelpBubbleView* bubble) {
            return bubble->arrow();
          },
          views::BubbleBorder::TOP_RIGHT),
      PressButton(kContextualTasksEphemeralToolbarButtonElementId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonPromoInteractiveTest,
                       PromoAbortsWhenButtonHides) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      // If the button hides before its show animation completes (e.g. task is
      // disassociated immediately on close), the promo should not be requested.
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(), RemoveTaskFromTab(0),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      CheckPromoRequested(
          feature_engagement::kIPHContextualTasksEphemeralToolbarButtonFeature,
          false),
      EnsureNotPresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      // When the button stays visible long enough for the show animation to
      // finish, the promo appears and is aborted if the button later hides.
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      WaitForPromo(
          feature_engagement::kIPHContextualTasksEphemeralToolbarButtonFeature),
      RemoveTaskFromTab(0),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckPromoActive(
          feature_engagement::kIPHContextualTasksEphemeralToolbarButtonFeature,
          false));
}

class ContextualTasksEphemeralButtonPinningV2PromoInteractiveTest
    : public ContextualTasksEphemeralButtonPromoInteractiveTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        ContextualTasksEphemeralButtonPromoInteractiveTest::
            GetEnabledFeatures();
    enabled_features.push_back(
        {contextual_tasks::kContextualTasksPinningV2, {}});
    return enabled_features;
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonPinningV2PromoInteractiveTest,
    ShowsPromoOnSidePanelCloseNotOpen) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      // Finish the button's show animation while the side panel is still open
      // and verify the ephemeral promo is not requested on open.
      WithView(kContextualTasksEphemeralToolbarButtonElementId,
               [](views::View* view) {
                 if (view->layer() && view->layer()->GetAnimator()) {
                   view->layer()->GetAnimator()->StopAnimating();
                 }
               }),
      CheckPromoRequested(
          feature_engagement::kIPHContextualTasksEphemeralToolbarButtonFeature,
          false),
      EnsureNotPresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      // Closing the side panel transitions the button into the ephemeral state
      // and triggers the promo.
      SimulateClosingContextualTaskSidePanel(),
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
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      NavigateWebContents(kFirstTab, GURL(chrome::kChromeUIContextualTasksURL)),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      Check([this]() {
        content::WebContents* const web_contents =
            browser()->GetTabStripModel()->GetWebContentsAt(0);
        SessionID session_id =
            sessions::SessionTabHelper::IdForTab(web_contents);
        return !GetContextualTasksService()
                    ->GetContextualTaskForTab(session_id)
                    .has_value();
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       NavigatingWhileSidePanelOpenDoesNotDisassociateTab) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      NavigateWebContents(
          kFirstTab,
          embedded_test_server()->GetURL("example.com", "/title2.html")),
      Check([this]() {
        content::WebContents* const web_contents =
            browser()->GetTabStripModel()->GetWebContentsAt(0);
        SessionID session_id =
            sessions::SessionTabHelper::IdForTab(web_contents);
        return GetContextualTasksService()
            ->GetContextualTaskForTab(session_id)
            .has_value();
      }),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId));
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
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.Shown."
                         "ActivePanelCollapsed"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.Shown.ActivePanelCollapsed",
            true, 1);
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
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration", 1);
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration.Clicked", 1);
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "ContextualTasks.PermanentToolbarButton.UserAction."
                         "OpenSidePanel"));
      }),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId));
}

class ContextualTasksEphemeralButtonPinningV2InteractiveTest
    : public ContextualTasksEphemeralButtonInteractiveTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        ContextualTasksEphemeralButtonInteractiveTest::GetEnabledFeatures();
    enabled_features.push_back(
        {contextual_tasks::kContextualTasksPinningV2, {}});
    return enabled_features;
  }

  auto SetContextualTasksRightAligned(bool right_aligned) {
    return Do([this, right_aligned]() {
      ScopedDictPrefUpdate(browser()->GetProfile()->GetPrefs(),
                           prefs::kSidePanelAlignmentOverrides)
          ->Set("kContextualTasks", right_aligned);
      browser()->GetProfile()->GetPrefs()->SetBoolean(
          prefs::kSidePanelHorizontalAlignment, right_aligned);
    });
  }

  auto CheckButtonIsInRightDock() {
    return CheckView(kContextualTasksEphemeralToolbarButtonElementId,
                     [this](ContextualTasksButton* button) {
                       if (button->GetShape() !=
                           contextual_tasks::EphemeralButtonShape::kCircle) {
                         return false;
                       }
                       BrowserView* browser_view =
                           BrowserView::GetBrowserViewForBrowser(browser());
                       ToolbarView* toolbar = browser_view->toolbar();
                       views::View* anchor = nullptr;
                       if (toolbar->GetGlicButton() &&
                           toolbar->GetGlicButton()->GetVisible()) {
                         anchor = toolbar->GetGlicButton();
                       } else if (toolbar->avatar_toolbar_button()) {
                         anchor = toolbar->avatar_toolbar_button();
                       }
                       if (anchor) {
                         auto button_idx = toolbar->GetIndexOf(button);
                         auto anchor_idx = toolbar->GetIndexOf(anchor);
                         return button_idx.has_value() &&
                                anchor_idx.has_value() &&
                                *button_idx == *anchor_idx - 1;
                       }
                       return true;
                     });
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonPinningV2InteractiveTest,
                       EphemeralButtonDoesNotShowInIncognito) {
  BrowserWindowInterface* const incognito_browser =
      CreateIncognitoBrowser(browser()->GetProfile());
  SidePanelUI::From(incognito_browser)->DisableAnimationsForTesting();

  auto* const incognito_service =
      contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
          incognito_browser->GetProfile());
  ASSERT_NE(incognito_service, nullptr);
  auto* const incognito_ephemeral_controller =
      ContextualTasksEphemeralButtonController::From(incognito_browser);
  ASSERT_NE(incognito_ephemeral_controller, nullptr);
  auto* const incognito_panel_controller =
      contextual_tasks::ContextualTasksPanelController::From(incognito_browser);
  ASSERT_NE(incognito_panel_controller, nullptr);

  content::WebContents* const web_contents =
      incognito_browser->GetActiveTabInterface()->GetContents();
  const SessionID session_id =
      sessions::SessionTabHelper::IdForTab(web_contents);

  contextual_tasks::ContextualTask task = incognito_service->CreateTask();
  incognito_service->AssociateTabWithTask(task.GetTaskId(), session_id);
  incognito_service->UpdateThreadForTask(
      task.GetTaskId(), contextual_tasks::ThreadType::kAiMode, "test_server_id",
      std::nullopt, "Test Title");

  incognito_panel_controller->Show();
  EXPECT_FALSE(incognito_ephemeral_controller->ShouldShowEphemeralButton());

  incognito_panel_controller->Close();
  EXPECT_FALSE(incognito_ephemeral_controller->ShouldShowEphemeralButton());

  RunTestSequence(InContext(
      BrowserView::GetBrowserViewForBrowser(incognito_browser)
          ->GetElementContext(),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId)));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonPinningV2InteractiveTest,
    AlwaysShowsAsCircleButtonInRightDockRegardlessOfSidePanelAlignment) {
  RunTestSequence(
      SignIntoEligibleAccount(), SetContextualTasksRightAligned(false),
      InstrumentTab(kFirstTab), AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateOpeningContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      CheckButtonIsInRightDock(), SetContextualTasksRightAligned(true),
      EnsurePresent(kContextualTasksEphemeralToolbarButtonElementId),
      CheckButtonIsInRightDock());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonPinningV2InteractiveTest,
                       ButtonShowsAndHighlightsWhileSidePanelIsOpen) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  const std::u16string expected_open_tooltip = l10n_util::GetStringUTF16(
      IDS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CLOSE_TOOL_TIP);
  const std::u16string expected_closed_tooltip = l10n_util::GetStringUTF16(
      IDS_CONTEXTUAL_TASKS_SIDE_PANEL_REOPEN_TOOL_TIP);

  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateOpeningContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.Shown."
                         "ActivePanelOpen"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.Shown.ActivePanelOpen",
            true, 1);
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.Shown."
                         "ActivePanelCollapsed"));
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.Shown.ActivePanelCollapsed",
            0);
      }),
      CheckViewProperty(kContextualTasksEphemeralToolbarButtonElementId,
                        &ContextualTasksButton::IsHighlighted, true),
      CheckViewProperty(kContextualTasksEphemeralToolbarButtonElementId,
                        &ContextualTasksButton::GetDropShadowLayerForTesting,
                        testing::IsNull()),
      CheckViewProperty(kContextualTasksEphemeralToolbarButtonElementId,
                        &views::View::GetTooltipText, expected_open_tooltip),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.Shown."
                         "ActivePanelCollapsed"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.Shown.ActivePanelCollapsed",
            true, 1);
      }),
      CheckViewProperty(kContextualTasksEphemeralToolbarButtonElementId,
                        &ContextualTasksButton::IsHighlighted, false),
      CheckViewProperty(kContextualTasksEphemeralToolbarButtonElementId,
                        &ContextualTasksButton::GetDropShadowLayerForTesting,
                        testing::IsNull()),
      CheckViewProperty(kContextualTasksEphemeralToolbarButtonElementId,
                        &views::View::GetTooltipText, expected_closed_tooltip),
      PressButton(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "OpenSidePanel"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction.OpenSidePanel",
            true, 1);
        EXPECT_EQ(2, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.Shown."
                         "ActivePanelOpen"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.Shown.ActivePanelOpen",
            true, 2);
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration", 1);
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration.Clicked", 1);
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "ContextualTasks.PermanentToolbarButton.UserAction."
                         "OpenSidePanel"));
      }),
      EnsurePresent(kContextualTasksEphemeralToolbarButtonElementId),
      CheckViewProperty(kContextualTasksEphemeralToolbarButtonElementId,
                        &ContextualTasksButton::IsHighlighted, true),
      CheckViewProperty(kContextualTasksEphemeralToolbarButtonElementId,
                        &views::View::GetTooltipText, expected_open_tooltip),
      PressButton(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "CloseSidePanel"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction.CloseSidePanel",
            true, 1);
        EXPECT_EQ(2, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.Shown."
                         "ActivePanelCollapsed"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.Shown.ActivePanelCollapsed",
            true, 2);
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "ContextualTasks.PermanentToolbarButton.UserAction."
                         "CloseSidePanel"));
      }),
      EnsurePresent(kContextualTasksEphemeralToolbarButtonElementId),
      CheckViewProperty(kContextualTasksEphemeralToolbarButtonElementId,
                        &ContextualTasksButton::IsHighlighted, false),
      CheckViewProperty(kContextualTasksEphemeralToolbarButtonElementId,
                        &views::View::GetTooltipText, expected_closed_tooltip));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonPinningV2InteractiveTest,
    LensOverlayEntryPointHighlightsPinButtonInsteadOfLensButton) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), Do([&]() {
        contextual_tasks::ContextualTasksPanelController::From(browser())->Show(
            /*transition_from_tab=*/false,
            omnibox::ChromeAimEntryPoint::
                DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      // When unpinned, the ephemeral pin button should be shown and
      // highlighted, and the Lens toolbar button must not be highlighted.
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      CheckViewProperty(kContextualTasksEphemeralToolbarButtonElementId,
                        &ContextualTasksButton::IsHighlighted, true),
      Check([&]() {
        auto* pinned_actions =
            ToolbarButtonProvider::From(browser())->GetPinnedToolbarActions();
        return pinned_actions && !pinned_actions->IsActionHighlighted(
                                     kActionSidePanelShowLensOverlayResults);
      }),
      SimulateClosingContextualTaskSidePanel(),
      CheckViewProperty(kContextualTasksEphemeralToolbarButtonElementId,
                        &ContextualTasksButton::IsHighlighted, false),
      // Pin the Contextual Tasks button permanently and reopen from the Lens
      // entry point via InitSidePanelWithGhostLoader (which runs when a closed
      // side panel still holds cached WebContents): the side panel should
      // reopen and the permanent pin button should be highlighted instead of
      // the Lens toolbar button.
      Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
        contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
            browser()->GetProfile())
            ->InitSidePanelWithGhostLoader(
                browser(), browser()->GetActiveTabInterface(), nullptr,
                omnibox::ChromeAimEntryPoint::
                    DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
      }),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      Check([&]() {
        auto* pinned_actions =
            ToolbarButtonProvider::From(browser())->GetPinnedToolbarActions();
        return pinned_actions &&
               pinned_actions->IsActionHighlighted(
                   kActionSidePanelShowContextualTasks) &&
               !pinned_actions->IsActionHighlighted(
                   kActionSidePanelShowLensOverlayResults);
      }));
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
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration", 0);
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration.Clicked", 0);
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
        GetEnabledFeatures(), {contextual_tasks::kContextualTasks});
    InteractiveBrowserTest::SetUp();
  }

  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{contextual_tasks::kContextualTasksSidePanel, {}},
            {contextual_tasks::kContextualTasksEphemeralBrandedEntryPoint,
             {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}},
            {contextual_tasks::kEnableContextualTasksPinButtonInToolbar, {}}};
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

class ContextualTasksEphemeralButtonCobrowseDisabledPinningV2InteractiveTest
    : public ContextualTasksEphemeralButtonCobrowseDisabledInteractiveTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        ContextualTasksEphemeralButtonCobrowseDisabledInteractiveTest::
            GetEnabledFeatures();
    enabled_features.push_back(
        {contextual_tasks::kContextualTasksPinningV2, {}});
    return enabled_features;
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonCobrowseDisabledPinningV2InteractiveTest,
    PermanentPinButtonTogglesExistingTaskAndOpensZeroStateWhenNone) {
  base::Uuid initial_task_id;
  base::Uuid resumed_task_id;
  base::Uuid pinned_resumed_task_id;
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
      // Pin to toolbar and press permanent button: should toggle existing task
      // open because tab 0 already has an associated session.
      Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      Do([&]() {
        tabs::TabInterface* tab =
            browser()->GetTabStripModel()->GetTabAtIndex(0);
        pinned_resumed_task_id =
            GetContextualTasksService()
                ->GetContextualTaskForTab(
                    sessions::SessionTabHelper::IdForTab(tab->GetContents()))
                ->GetTaskId();
        EXPECT_EQ(initial_task_id, pinned_resumed_task_id);
      }),
      // Switch to tab 1 (which has no associated task) and press permanent
      // button: should open a new zero-state thread.
      SelectTab(kTabStripElementId, 1),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      Do([&]() {
        tabs::TabInterface* tab =
            browser()->GetTabStripModel()->GetTabAtIndex(1);
        new_task_id =
            GetContextualTasksService()
                ->GetContextualTaskForTab(
                    sessions::SessionTabHelper::IdForTab(tab->GetContents()))
                ->GetTaskId();
        EXPECT_NE(initial_task_id, new_task_id);
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonPinningV2InteractiveTest,
                       LogsPermanentEphemeralMetricsForContinueVsNewThread) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0), Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      // Pressing the permanent button on tab 0 (no active task) starts a new
      // thread and logs NewThread.
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.PermanentToolbarButton.UserAction."
                         "OpenSidePanel"));
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.PermanentToolbarButton.UserAction."
                         "OpenSidePanel.NewThread"));
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "ContextualTasks.PermanentToolbarButton.UserAction."
                         "OpenSidePanel.ContinueThread"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.PermanentToolbarButton.UserAction."
            "OpenSidePanel.ThreadAction",
            contextual_tasks::ContextualTasksPanelController::
                PermanentToolbarButtonThreadAction::kNewThread,
            1);
      }),
      // Close the side panel so the permanent button enters its ephemeral
      // task-resumption state on tab 0.
      SimulateClosingContextualTaskSidePanel(),
      // Pressing the permanent button on tab 0 now continues the existing
      // thread, logs ContinueThread, and does NOT log NewThread.
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      Do([&]() {
        EXPECT_EQ(2, user_action_tester.GetActionCount(
                         "ContextualTasks.PermanentToolbarButton.UserAction."
                         "OpenSidePanel"));
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.PermanentToolbarButton.UserAction."
                         "OpenSidePanel.NewThread"));
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.PermanentToolbarButton.UserAction."
                         "OpenSidePanel.ContinueThread"));
        histogram_tester.ExpectBucketCount(
            "ContextualTasks.PermanentToolbarButton.UserAction."
            "OpenSidePanel.ThreadAction",
            contextual_tasks::ContextualTasksPanelController::
                PermanentToolbarButtonThreadAction::kNewThread,
            1);
        histogram_tester.ExpectBucketCount(
            "ContextualTasks.PermanentToolbarButton.UserAction."
            "OpenSidePanel.ThreadAction",
            contextual_tasks::ContextualTasksPanelController::
                PermanentToolbarButtonThreadAction::kContinueThread,
            1);
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.PermanentToolbarButton.UserAction."
            "OpenSidePanel.ThreadAction",
            2);
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       ButtonShowsWhenSignedOutAndAimIneligible) {
  RunTestSequence(
      SetIsAimEligible(false), InstrumentTab(kFirstTab),
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
    ContextualTasksEphemeralButtonInteractiveTest,
    ButtonHidesWhenSidePanelIsRightAlignedAndRightDockDisabled) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      // Verify button is visible and docked at leading index 0.
      Do([&]() {
        ToolbarView* toolbar =
            BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
        ToolbarButton* button = toolbar->contextual_tasks_button();
        ASSERT_NE(button, nullptr);
        EXPECT_TRUE(button->GetVisible());
        EXPECT_EQ(toolbar->GetIndexOf(button), 0u);
      }),
      // Set Contextual Tasks side panel alignment to the right. Since
      // right-dock is disabled by default, the ephemeral button should be
      // hidden.
      Do([&]() {
        base::DictValue new_overrides =
            browser()
                ->GetProfile()
                ->GetPrefs()
                ->GetDict(prefs::kSidePanelAlignmentOverrides)
                .Clone();
        new_overrides.Set(
            SidePanelEntryIdToString(SidePanelEntryId::kContextualTasks), true);
        browser()->GetProfile()->GetPrefs()->SetDict(
            prefs::kSidePanelAlignmentOverrides, std::move(new_overrides));
      }),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        ToolbarView* toolbar =
            BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
        ToolbarButton* button = toolbar->contextual_tasks_button();
        ASSERT_NE(button, nullptr);
        EXPECT_FALSE(button->GetVisible());
      }),
      // Restore alignment to the left. The button becomes visible again.
      Do([&]() {
        base::DictValue new_overrides =
            browser()
                ->GetProfile()
                ->GetPrefs()
                ->GetDict(prefs::kSidePanelAlignmentOverrides)
                .Clone();
        new_overrides.Set(
            SidePanelEntryIdToString(SidePanelEntryId::kContextualTasks),
            false);
        browser()->GetProfile()->GetPrefs()->SetDict(
            prefs::kSidePanelAlignmentOverrides, std::move(new_overrides));
      }),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        ToolbarView* toolbar =
            BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
        ToolbarButton* button = toolbar->contextual_tasks_button();
        ASSERT_NE(button, nullptr);
        EXPECT_TRUE(button->GetVisible());
        EXPECT_EQ(toolbar->GetIndexOf(button), 0u);
      }));
}

class ContextualTasksEphemeralButtonRightDockInteractiveTest
    : public ContextualTasksEphemeralButtonInteractiveTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {
        {contextual_tasks::kContextualTasks,
         {{"ContextualTasksExpandButtonOptions", "toolbar-close-button"}}},
        {contextual_tasks::kContextualTasksEphemeralBrandedEntryPoint,
         {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"},
          {"enable-right-hand-contextual-tasks-ephemeral-button", "true"}}},
        {contextual_tasks::kContextualTasksHideCloseButtonInVerticalTabs, {}},
        {contextual_tasks::kEnableContextualTasksPinButtonInToolbar, {}}};
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonRightDockInteractiveTest,
                       ButtonRightDocksWhenSidePanelIsRightAligned) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      // Verify button is visible and docked at leading index 0.
      Do([&]() {
        ToolbarView* toolbar =
            BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
        ToolbarButton* button = toolbar->contextual_tasks_button();
        ASSERT_NE(button, nullptr);
        EXPECT_TRUE(button->GetVisible());
        EXPECT_EQ(toolbar->GetIndexOf(button), 0u);
      }),
      // Set Contextual Tasks side panel alignment to the right. The button
      // should move to the trailing edge and stay visible.
      Do([&]() {
        base::DictValue new_overrides =
            browser()
                ->GetProfile()
                ->GetPrefs()
                ->GetDict(prefs::kSidePanelAlignmentOverrides)
                .Clone();
        new_overrides.Set(
            SidePanelEntryIdToString(SidePanelEntryId::kContextualTasks), true);
        browser()->GetProfile()->GetPrefs()->SetDict(
            prefs::kSidePanelAlignmentOverrides, std::move(new_overrides));
      }),
      EnsurePresent(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        ToolbarView* toolbar =
            BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
        ToolbarButton* button = toolbar->contextual_tasks_button();
        ASSERT_NE(button, nullptr);
        EXPECT_TRUE(button->GetVisible());
        EXPECT_EQ(toolbar->GetIndexOf(button), toolbar->children().size() - 1);
      }),
      // Restore alignment to the left. The button moves back to the leading
      // edge.
      Do([&]() {
        base::DictValue new_overrides =
            browser()
                ->GetProfile()
                ->GetPrefs()
                ->GetDict(prefs::kSidePanelAlignmentOverrides)
                .Clone();
        new_overrides.Set(
            SidePanelEntryIdToString(SidePanelEntryId::kContextualTasks),
            false);
        browser()->GetProfile()->GetPrefs()->SetDict(
            prefs::kSidePanelAlignmentOverrides, std::move(new_overrides));
      }),
      EnsurePresent(kContextualTasksEphemeralToolbarButtonElementId), Do([&]() {
        ToolbarView* toolbar =
            BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
        ToolbarButton* button = toolbar->contextual_tasks_button();
        ASSERT_NE(button, nullptr);
        EXPECT_TRUE(button->GetVisible());
        EXPECT_EQ(toolbar->GetIndexOf(button), 0u);
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonInteractiveTest,
                       DropShadowShowsAndAlignsOnFirstAndSubsequentOpen) {
  raw_ptr<ContextualTasksButton> button = nullptr;

  auto VerifyDropShadow = [&button]() {
    EXPECT_NE(button, nullptr);
    if (!button) {
      return;
    }
    ui::Layer* shadow_layer = button->GetDropShadowLayerForTesting();
    EXPECT_NE(shadow_layer, nullptr);
    if (!shadow_layer) {
      return;
    }
    EXPECT_EQ(shadow_layer->GetTargetOpacity(), 1.0f);
    EXPECT_EQ(shadow_layer->parent(), button->layer()->parent());

    gfx::Rect expected_bounds = button->GetLocalBounds();
    expected_bounds.Outset(ContextualTasksButton::kShadowOutset);
    expected_bounds.Offset(button->layer()->bounds().OffsetFromOrigin());
    EXPECT_EQ(shadow_layer->bounds(), expected_bounds);
  };

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
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      // Capture the button pointer and verify drop shadow is rendered and
      // aligned properly on first show.
      CheckView(kContextualTasksEphemeralToolbarButtonElementId,
                [&button](ContextualTasksButton* b) {
                  button = b;
                  return true;
                }),
      Do(VerifyDropShadow),
      // Change widget bounds and verify drop shadow layer updates and remains
      // aligned with the button.
      Do([this]() {
        views::Widget* widget =
            BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
        gfx::Rect bounds = widget->GetWindowBoundsInScreen();
        bounds.set_width(bounds.width() + 40);
        widget->SetBounds(bounds);
      }),
      Do(VerifyDropShadow),
      // Open the side panel: ephemeral button should hide and the drop shadow
      // layer should be cleared.
      SimulateOpeningContextualTaskSidePanel(),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      Do([&button]() {
        EXPECT_NE(button, nullptr);
        if (button) {
          EXPECT_EQ(button->GetDropShadowLayerForTesting(), nullptr);
        }
      }),
      // Close side panel again: button shows and drop shadow reappears
      // correctly on subsequent shows.
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      Do(VerifyDropShadow),
      // Switch to tab 1 (which has no task): button hides and drop shadow is
      // cleared.
      SelectTab(kTabStripElementId, 1),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      Do([&button]() {
        EXPECT_NE(button, nullptr);
        if (button) {
          EXPECT_EQ(button->GetDropShadowLayerForTesting(), nullptr);
        }
      }),
      // Switch back to tab 0: button and drop shadow are restored.
      SelectTab(kTabStripElementId, 0),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      Do(VerifyDropShadow));
}

class ContextualTasksCircularEphemeralButtonNextToBatterySaverInteractiveTest
    : public ContextualTasksEphemeralButtonInteractiveTestMixin<
          InteractiveFeaturePromoTest> {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {
        {contextual_tasks::kContextualTasks,
         {{"ContextualTasksExpandButtonOptions", "toolbar-close-button"}}},
        {contextual_tasks::kContextualTasksEphemeralBrandedEntryPoint,
         {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"},
          {contextual_tasks::kEnableCircularEphemeralButtonNextToBatterySaver
               .name,
           "true"}}},
        {contextual_tasks::kContextualTasksHideCloseButtonInVerticalTabs, {}},
        {contextual_tasks::kEnableContextualTasksPinButtonInToolbar, {}}};
  }

  auto SetContextualTasksRightAligned(bool right_aligned) {
    return Do([this, right_aligned]() {
      ScopedDictPrefUpdate(browser()->GetProfile()->GetPrefs(),
                           prefs::kSidePanelAlignmentOverrides)
          ->Set("kContextualTasks", right_aligned);
      browser()->GetProfile()->GetPrefs()->SetBoolean(
          prefs::kSidePanelHorizontalAlignment, right_aligned);
    });
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksCircularEphemeralButtonNextToBatterySaverInteractiveTest,
    ShowsAsCircleButtonNextToBatterySaver) {
  RunTestSequence(
      SignIntoEligibleAccount(), SetContextualTasksRightAligned(true),
      InstrumentTab(kFirstTab), AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateOpeningContextualTaskSidePanel(),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      CheckView(kContextualTasksEphemeralToolbarButtonElementId,
                [](ContextualTasksButton* button) {
                  return button->GetShape() ==
                         contextual_tasks::EphemeralButtonShape::kCircle;
                }),
      CheckView(kContextualTasksEphemeralToolbarButtonElementId,
                [this](ContextualTasksButton* button) {
                  BrowserView* browser_view =
                      BrowserView::GetBrowserViewForBrowser(browser());
                  ToolbarView* toolbar = browser_view->toolbar();
                  views::View* anchor = nullptr;
                  if (toolbar->GetGlicButton() &&
                      toolbar->GetGlicButton()->GetVisible()) {
                    anchor = toolbar->GetGlicButton();
                  } else if (toolbar->avatar_toolbar_button()) {
                    anchor = toolbar->avatar_toolbar_button();
                  }
                  if (anchor) {
                    auto button_idx = toolbar->GetIndexOf(button);
                    auto anchor_idx = toolbar->GetIndexOf(anchor);
                    return button_idx.has_value() && anchor_idx.has_value() &&
                           *button_idx == *anchor_idx - 1;
                  }
                  return true;
                }));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksCircularEphemeralButtonNextToBatterySaverInteractiveTest,
    ShowsAsLeftFlatEdgeButtonWhenSidePanelOnLeft) {
  RunTestSequence(
      SignIntoEligibleAccount(), SetContextualTasksRightAligned(false),
      InstrumentTab(kFirstTab), AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateOpeningContextualTaskSidePanel(),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      CheckView(kContextualTasksEphemeralToolbarButtonElementId,
                [](ContextualTasksButton* button) {
                  return button->GetShape() ==
                         contextual_tasks::EphemeralButtonShape::kFlatEdgeLeft;
                }),
      CheckView(kContextualTasksEphemeralToolbarButtonElementId,
                [](ContextualTasksButton* button) {
                  auto* toolbar =
                      views::AsViewClass<ToolbarView>(button->parent());
                  return toolbar && toolbar->GetIndexOf(button) == 0u;
                }));
}

// Regression test for b/552070324: Destroying a layer in a button's region
// must not leave a dangling pointer in views::View's layers, which would cause
// a SIGSEGV in ui::LayerAnimator::GetTransitionDuration() when the button
// becomes visible.
IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonInteractiveTest,
    LayerDestroyedDoesNotLeaveDanglingLayerInRegionAndCrashOnShow) {
  raw_ptr<ContextualTasksButton> button = nullptr;
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      CheckView(kContextualTasksEphemeralToolbarButtonElementId,
                [&button](ContextualTasksButton* b) {
                  button = b;
                  return true;
                }),
      Do([&button]() {
        ASSERT_NE(button, nullptr);
        auto extra_layer = std::make_unique<ui::LayerSolidColor>();
        button->views::View::AddLayerToRegion(extra_layer.get(),
                                              views::LayerRegion::kBelow);
        extra_layer.reset();
        button->SetVisible(false);
        button->SetVisible(true);
      }));
}

class ContextualTasksEphemeralButtonAutoDismissInteractiveTest
    : public ContextualTasksEphemeralButtonInteractiveTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        ContextualTasksEphemeralButtonInteractiveTest::GetEnabledFeatures();
    enabled_features.push_back(
        {contextual_tasks::kContextualTasksEphemeralButtonAutoDismiss,
         {{"ContextualTasksEphemeralButtonAutoDismissTimeMinutes", "120"}}});
    return enabled_features;
  }

  void SetUpOnMainThread() override {
    ContextualTasksEphemeralButtonInteractiveTest::SetUpOnMainThread();
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    if (auto* controller =
            ContextualTasksEphemeralButtonController::From(browser())) {
      controller->SetTaskRunnerForTesting(task_runner_);
    }
  }

  void TearDownOnMainThread() override {
    if (auto* controller =
            ContextualTasksEphemeralButtonController::From(browser())) {
      controller->SetTaskRunnerForTesting(nullptr);
    }
    task_runner_.reset();
    ContextualTasksEphemeralButtonInteractiveTest::TearDownOnMainThread();
  }

  auto FastForwardBy(base::TimeDelta delta) {
    return Do([this, delta]() { task_runner_->FastForwardBy(delta); });
  }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonAutoDismissInteractiveTest,
                       AutoDismissHidesButtonAndDisassociatesTaskAfterTimeout) {
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
      // Fast-forward the mock clock so the auto-dismiss timer fires and hides
      // the ephemeral button.
      FastForwardBy(base::Minutes(120)),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton."
                         "AutoDismissed"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.AutoDismissed", 120, 1);
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration", 1);
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.ShownDuration."
            "AutoDismissed",
            1);
      }),
      Check([this]() {
        content::WebContents* const web_contents =
            browser()->GetTabStripModel()->GetWebContentsAt(0);
        SessionID session_id =
            sessions::SessionTabHelper::IdForTab(web_contents);
        return !GetContextualTasksService()
                    ->GetContextualTaskForTab(session_id)
                    .has_value();
      }));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksEphemeralButtonAutoDismissInteractiveTest,
                       ReopeningSidePanelCancelsAutoDismissTimer) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      Check([this]() {
        content::WebContents* const web_contents =
            browser()->GetTabStripModel()->GetWebContentsAt(0);
        SessionID session_id =
            sessions::SessionTabHelper::IdForTab(web_contents);
        auto* controller =
            ContextualTasksEphemeralButtonController::From(browser());
        return controller &&
               controller->HasAutoDismissTimerForTesting(session_id);
      }),
      // Reopening the side panel cancels the pending auto-dismiss timer.
      SimulateOpeningContextualTaskSidePanel(),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      Check([this]() {
        content::WebContents* const web_contents =
            browser()->GetTabStripModel()->GetWebContentsAt(0);
        SessionID session_id =
            sessions::SessionTabHelper::IdForTab(web_contents);
        auto* controller =
            ContextualTasksEphemeralButtonController::From(browser());
        return controller &&
               !controller->HasAutoDismissTimerForTesting(session_id);
      }),
      // Closing the side panel again restarts the auto-dismiss timer, which
      // then hides the button when it expires.
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      FastForwardBy(base::Minutes(120)),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonAutoDismissInteractiveTest,
    NavigatingTabWhenPanelClosedDismissesButtonForNavigatedTabOnly) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      // Associate the second tab with the same task after the panel closed.
      Do([this]() {
        content::WebContents* const tab0 =
            browser()->GetTabStripModel()->GetWebContentsAt(0);
        content::WebContents* const tab1 =
            browser()->GetTabStripModel()->GetWebContentsAt(1);
        std::optional<contextual_tasks::ContextualTask> task =
            GetContextualTasksService()->GetContextualTaskForTab(
                sessions::SessionTabHelper::IdForTab(tab0));
        ASSERT_TRUE(task.has_value());
        GetContextualTasksService()->AssociateTabWithTask(
            task->GetTaskId(), sessions::SessionTabHelper::IdForTab(tab1));
      }),
      // Navigate the first tab away while the panel is closed: the first tab
      // loses the button and is disassociated, while the second tab keeps the
      // button and gets an auto-dismiss timer.
      NavigateWebContents(
          kFirstTab,
          embedded_test_server()->GetURL("example.com", "/title2.html")),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      Check([this]() {
        content::WebContents* const tab0 =
            browser()->GetTabStripModel()->GetWebContentsAt(0);
        content::WebContents* const tab1 =
            browser()->GetTabStripModel()->GetWebContentsAt(1);
        SessionID tab0_id = sessions::SessionTabHelper::IdForTab(tab0);
        SessionID tab1_id = sessions::SessionTabHelper::IdForTab(tab1);
        auto* controller =
            ContextualTasksEphemeralButtonController::From(browser());
        auto* panel_controller =
            contextual_tasks::ContextualTasksPanelController::From(browser());
        return !GetContextualTasksService()
                    ->GetContextualTaskForTab(tab0_id)
                    .has_value() &&
               GetContextualTasksService()
                   ->GetContextualTaskForTab(tab1_id)
                   .has_value() &&
               controller &&
               !controller->HasAutoDismissTimerForTesting(tab0_id) &&
               controller->HasAutoDismissTimerForTesting(tab1_id) &&
               panel_controller &&
               panel_controller->GetPanelWebContentsList().size() == 1u;
      }),
      SelectTab(kTabStripElementId, 1),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      FastForwardBy(base::Minutes(120)),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      Check([this]() {
        auto* panel_controller =
            contextual_tasks::ContextualTasksPanelController::From(browser());
        return panel_controller &&
               panel_controller->GetPanelWebContentsList().empty();
      }));
}

class ContextualTasksEphemeralButtonCloseSidePanelContextMenuInteractiveTest
    : public ContextualTasksEphemeralButtonInteractiveTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {
        {contextual_tasks::kContextualTasks,
         {{"ContextualTasksExpandButtonOptions", "toolbar-close-button"}}},
        {contextual_tasks::kContextualTasksEphemeralBrandedEntryPoint,
         {{"ContextualTasksEntryPoint", "toolbar-ephemeral-branded"}}},
        {contextual_tasks::kContextualTasksHideCloseButtonInVerticalTabs, {}},
        {contextual_tasks::kEnableContextualTasksPinButtonInToolbar, {}},
        {contextual_tasks::kContextualTasksEphemeralButtonContextMenu,
         {{"ContextualTasksEphemeralButtonContextMenuCloseSidePanel",
           "true"}}}};
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonCloseSidePanelContextMenuInteractiveTest,
    RightClickShowsCloseSidePanelAndRemovesButton) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      ShowToolbarButtonContextMenu(
          kContextualTasksEphemeralToolbarButtonElementId),
      WaitForShow(
          ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem),
      WaitForShow(kPinnedActionToolbarPinElementId),
      WaitForShow(kPinnedActionToolbarCustomizeElementId),
      CheckView(ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem,
                [](views::MenuItemView* menu_item) {
                  return !menu_item->GetIcon().IsEmpty();
                }),
      CheckViewProperty(
          ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem,
          &views::MenuItemView::title,
          l10n_util::GetStringUTF16(
              IDS_CONTEXTUAL_TASKS_EPHEMERAL_BUTTON_CONTEXT_MENU_CLOSE)),
      SelectMenuItem(
          ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonCloseSidePanelContextMenuInteractiveTest,
    RightClickEphemeralButtonPinToToolbarPinsAndHidesEphemeralButton) {
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
      ShowToolbarButtonContextMenu(
          kContextualTasksEphemeralToolbarButtonElementId),
      WaitForShow(kPinnedActionToolbarPinElementId),
      SelectMenuItem(kPinnedActionToolbarPinElementId),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "PinByContextMenu"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction."
            "PinByContextMenu",
            true, 1);
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "Actions.PinnedToolbarButton.Pinned.ByContextMenu."
                         "kActionSidePanelShowContextualTasks"));
      }),
      CheckResult(
          [this]() {
            return PinnedToolbarActionsModel::Get(browser()->GetProfile())
                ->Contains(kActionSidePanelShowContextualTasks);
          },
          true));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonCloseSidePanelContextMenuInteractiveTest,
    RightClickPinnedButtonDoesNotShowCloseSidePanelOption) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0), Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      ShowToolbarButtonContextMenu(
          kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WaitForShow(kPinnedActionToolbarUnpinElementId),
      WaitForShow(kPinnedActionToolbarCustomizeElementId),
      EnsureNotPresent(
          ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonCloseSidePanelContextMenuInteractiveTest,
    RightClickWhenNotPinningEligibleHidesPinAndCustomizeOptions) {
  RunTestSequence(
      SignIntoEligibleAccount(), SetIsFuseboxEligible(false),
      InstrumentTab(kFirstTab), AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kContextualTasksEphemeralToolbarButtonElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      ShowToolbarButtonContextMenu(
          kContextualTasksEphemeralToolbarButtonElementId),
      WaitForShow(
          ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem),
      EnsureNotPresent(kPinnedActionToolbarPinElementId),
      EnsureNotPresent(kPinnedActionToolbarUnpinElementId),
      EnsureNotPresent(kPinnedActionToolbarCustomizeElementId),
      SelectMenuItem(
          ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId));
}

class ContextualTasksPinnedButtonActiveIndicatorInteractiveTest
    : public ContextualTasksEphemeralButtonPinningV2InteractiveTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        ContextualTasksEphemeralButtonPinningV2InteractiveTest::
            GetEnabledFeatures();
    enabled_features.push_back(
        {contextual_tasks::kContextualTasksPinnedButtonActiveIndicator, {}});
    return enabled_features;
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksPinnedButtonActiveIndicatorInteractiveTest,
    ShowsActiveIndicatorOnlyWhenActiveTabHasOngoingTask) {
  raw_ptr<PinnedActionToolbarButton> pinned_button = nullptr;

  auto VerifyActiveIndicator = [&](bool expected_active) {
    ASSERT_NE(pinned_button, nullptr);
    actions::ActionItem* const action_item =
        actions::ActionManager::Get().FindAction(
            kActionSidePanelShowContextualTasks,
            BrowserActions::From(browser())->root_action_item());
    ASSERT_NE(action_item, nullptr);
    EXPECT_EQ(action_item->GetProperty(kActionItemUnderlineIndicatorKey),
              expected_active);
    ASSERT_NE(pinned_button->GetStatusIndicatorForTesting(), nullptr);
    EXPECT_EQ(pinned_button->GetStatusIndicatorForTesting()->GetVisible(),
              expected_active);
  };

  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0), Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WithView(kPinnedToolbarActionShowSidePanelContextualTasksElementId,
               [](views::View* view) {
                 if (auto* container =
                         views::AsViewClass<PinnedToolbarActionsContainer>(
                             view->parent())) {
                   container->GetAnimatingLayoutManager()->ResetLayout();
                 }
                 view->GetWidget()->LayoutRootViewIfNecessary();
               }),
      CheckView(kPinnedToolbarActionShowSidePanelContextualTasksElementId,
                [&pinned_button](PinnedActionToolbarButton* b) {
                  pinned_button = b;
                  return true;
                }),
      // Before opening the side panel on Tab 0, the pinned button should not
      // show the active indicator.
      Do([&]() { VerifyActiveIndicator(false); }),
      // Opening the side panel in zero state shows the active indicator while
      // open, but closing it in zero state clobbers eligibility and hides it.
      Do([&]() {
        contextual_tasks::ContextualTasksPanelController::From(browser())
            ->Show();
      }),
      Do([&]() { VerifyActiveIndicator(true); }),
      SimulateClosingContextualTaskSidePanel(),
      Do([&]() { VerifyActiveIndicator(false); }),
      RemoveTaskFromTab(0),
      // Create an ongoing task on Tab 0 and open/close the side panel: the
      // pinned button should show the active indicator (blue icon + underline).
      CreateTaskForTab(0),
      SimulateOpeningContextualTaskSidePanel(),
      Do([&]() { VerifyActiveIndicator(true); }),
      SimulateClosingContextualTaskSidePanel(),
      Do([&]() { VerifyActiveIndicator(true); }),
      // Switch to Tab 1 (no ongoing task, would lead to zero state): active
      // indicator should be hidden.
      SelectTab(kTabStripElementId, 1),
      Do([&]() { VerifyActiveIndicator(false); }),
      // Switch back to Tab 0 (ongoing task): active indicator should be
      // restored.
      SelectTab(kTabStripElementId, 0),
      Do([&]() { VerifyActiveIndicator(true); }),
      // Disassociate the task from Tab 0: active indicator should be cleared.
      RemoveTaskFromTab(0), Do([&]() { VerifyActiveIndicator(false); }),
      // Variant 1 (default): right-clicking the permanent pin button does not
      // show the "Close/Hide side panel" context menu option.
      ShowToolbarButtonContextMenu(
          kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WaitForShow(kPinnedActionToolbarUnpinElementId),
      WaitForShow(kPinnedActionToolbarCustomizeElementId),
      EnsureNotPresent(
          ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem));
}

class ContextualTasksPinnedButtonActiveIndicatorWithContextMenuInteractiveTest
    : public ContextualTasksPinnedButtonActiveIndicatorInteractiveTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        ContextualTasksEphemeralButtonPinningV2InteractiveTest::
            GetEnabledFeatures();
    enabled_features.push_back(
        {contextual_tasks::kContextualTasksPinnedButtonActiveIndicator,
         {{"ContextualTasksPinnedButtonContextMenuCloseSidePanel", "true"}}});
    return enabled_features;
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksPinnedButtonActiveIndicatorWithContextMenuInteractiveTest,
    RightClickPinnedButtonDoesNotShowCloseSidePanelWhenPinned) {
  raw_ptr<PinnedActionToolbarButton> pinned_button = nullptr;

  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0), Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WithView(kPinnedToolbarActionShowSidePanelContextualTasksElementId,
               [](views::View* view) {
                 if (auto* container =
                         views::AsViewClass<PinnedToolbarActionsContainer>(
                             view->parent())) {
                   container->GetAnimatingLayoutManager()->ResetLayout();
                 }
                 view->GetWidget()->LayoutRootViewIfNecessary();
               }),
      CheckView(kPinnedToolbarActionShowSidePanelContextualTasksElementId,
                [&pinned_button](PinnedActionToolbarButton* b) {
                  pinned_button = b;
                  return true;
                }),
      // Create an ongoing task on Tab 0 and open the side panel: the active
      // indicator should show, and right-clicking the permanent pin button
      // should not show "Close/Hide side panel" when pinned.
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      WaitForShow(
          ContextualTasksCloseTabButton::kContextualTasksCloseTabButton),
      Check([&]() {
        return pinned_button->GetStatusIndicatorForTesting() &&
               pinned_button->GetStatusIndicatorForTesting()->GetVisible();
      }),
      ShowToolbarButtonContextMenu(
          kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WaitForShow(kPinnedActionToolbarUnpinElementId),
      WaitForShow(kPinnedActionToolbarCustomizeElementId),
      EnsureNotPresent(
          ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem),
      SelectMenuItem(kPinnedActionToolbarCustomizeElementId),
      WaitForHide(kPinnedActionToolbarUnpinElementId),
      SimulateClosingContextualTaskSidePanel(),
      // Even when the side panel is closed with an active task, right-clicking
      // the pinned button should not show "Close/Hide side panel".
      ShowToolbarButtonContextMenu(
          kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WaitForShow(kPinnedActionToolbarUnpinElementId),
      WaitForShow(kPinnedActionToolbarCustomizeElementId),
      EnsureNotPresent(
          ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem),
      SelectMenuItem(kPinnedActionToolbarCustomizeElementId),
      WaitForHide(kPinnedActionToolbarUnpinElementId), Do([&]() {
        base::UserActionTester user_action_tester;
        base::HistogramTester histogram_tester;
        actions::ActionItem* const close_action =
            actions::ActionManager::Get().FindAction(
                kActionContextualTasksCloseSidePanel,
                BrowserActions::From(browser())->root_action_item());
        ASSERT_NE(close_action, nullptr);
        close_action->SetEnabled(true);
        close_action->InvokeAction();
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "RemoveByContextMenu"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction."
            "RemoveByContextMenu",
            true, 1);
      }));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksEphemeralButtonPinningV2InteractiveTest,
    CloseSidePanelOptionOnlyShownWhenSidePanelClosedAndNotPinned) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab), Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, false);
      }),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      // While the side panel is open and not pinned, right-clicking the button
      // should NOT show "Close/Hide side panel" or a separator line.
      CheckView(
          kContextualTasksEphemeralToolbarButtonElementId,
          [](ContextualTasksButton* button) {
            ui::MenuModel* model = button->menu_model();
            if (!model) {
              return false;
            }
            for (size_t i = 0; i < model->GetItemCount(); ++i) {
              if (model->GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR) {
                return false;
              }
            }
            return true;
          },
          true),
      ShowToolbarButtonContextMenu(
          kContextualTasksEphemeralToolbarButtonElementId),
      WaitForShow(kPinnedActionToolbarPinElementId),
      WaitForShow(kPinnedActionToolbarCustomizeElementId),
      EnsureNotPresent(
          ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem),
      Do([&]() {
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "OpenContextMenu"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction.OpenContextMenu",
            true, 1);
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "OpenContextMenu.CloseSidePanelOptionHidden"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction."
            "OpenContextMenu.CloseSidePanelOptionHidden",
            true, 1);
        EXPECT_EQ(0, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "OpenContextMenu.CloseSidePanelOptionVisible"));
        histogram_tester.ExpectTotalCount(
            "ContextualTasks.EphemeralToolbarButton.UserAction."
            "OpenContextMenu.CloseSidePanelOptionVisible",
            0);
      }),
      // Close the context menu and close the side panel.
      SelectMenuItem(kPinnedActionToolbarCustomizeElementId),
      WaitForHide(kPinnedActionToolbarPinElementId),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      // When the side panel is closed and not pinned, "Close/Hide side panel"
      // and the separator line SHOULD be shown.
      CheckView(
          kContextualTasksEphemeralToolbarButtonElementId,
          [](ContextualTasksButton* button) {
            ui::MenuModel* model = button->menu_model();
            if (!model) {
              return false;
            }
            bool has_separator = false;
            for (size_t i = 0; i < model->GetItemCount(); ++i) {
              if (model->GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR) {
                has_separator = true;
              }
            }
            return has_separator;
          },
          true),
      ShowToolbarButtonContextMenu(
          kContextualTasksEphemeralToolbarButtonElementId),
      WaitForShow(
          ContextualTasksEphemeralButtonMenuModel::kRemoveForTaskMenuItem),
      WaitForShow(kPinnedActionToolbarPinElementId),
      WaitForShow(kPinnedActionToolbarCustomizeElementId), Do([&]() {
        EXPECT_EQ(2, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "OpenContextMenu"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction.OpenContextMenu",
            true, 2);
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "OpenContextMenu.CloseSidePanelOptionHidden"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction."
            "OpenContextMenu.CloseSidePanelOptionHidden",
            true, 1);
        EXPECT_EQ(1, user_action_tester.GetActionCount(
                         "ContextualTasks.EphemeralToolbarButton.UserAction."
                         "OpenContextMenu.CloseSidePanelOptionVisible"));
        histogram_tester.ExpectUniqueSample(
            "ContextualTasks.EphemeralToolbarButton.UserAction."
            "OpenContextMenu.CloseSidePanelOptionVisible",
            true, 1);
      }));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksPinnedButtonActiveIndicatorInteractiveTest,
    EphemeralButtonShowsActiveIndicatorWithoutDropShadow) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab), Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, false);
      }),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      CheckView(
          kContextualTasksEphemeralToolbarButtonElementId,
          [](ContextualTasksButton* button) {
            return button->GetStatusIndicatorForTesting() != nullptr &&
                   button->GetStatusIndicatorForTesting()->GetVisible() &&
                   button->GetDropShadowLayerForTesting() == nullptr;
          },
          true),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      CheckView(
          kContextualTasksEphemeralToolbarButtonElementId,
          [](ContextualTasksButton* button) {
            return button->GetStatusIndicatorForTesting() != nullptr &&
                   button->GetStatusIndicatorForTesting()->GetVisible() &&
                   button->GetDropShadowLayerForTesting() == nullptr;
          },
          true));
}

class ContextualTasksPinnedIconChangedPromoInteractiveTest
    : public ContextualTasksEphemeralButtonPromoInteractiveTestBase {
 public:
  ContextualTasksPinnedIconChangedPromoInteractiveTest()
      : ContextualTasksEphemeralButtonPromoInteractiveTestBase(
            UseDefaultTrackerAllowingPromos(
                {feature_engagement::
                     kIPHContextualTasksPinnedIconChangedFeature})) {}
  ~ContextualTasksPinnedIconChangedPromoInteractiveTest() override = default;

  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        ContextualTasksEphemeralButtonPromoInteractiveTestBase::
            GetEnabledFeatures();
    const std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name != "PRE_ShowsPromoForPreviouslyPinnedUserInSuperGArm") {
      enabled_features.push_back(
          {contextual_tasks::kContextualTasksUnifyPinButtonLogos,
           {{"ContextualTasksUnifyPinButtonLogo", "super-g"}}});
    }
    return enabled_features;
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksPinnedIconChangedPromoInteractiveTest,
                       PRE_ShowsPromoForPreviouslyPinnedUserInSuperGArm) {
  RunTestSequence(
      SignIntoEligibleAccount(), Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPinnedIconChangedPromoInteractiveTest,
                       ShowsPromoForPreviouslyPinnedUserInSuperGArm) {
  RunTestSequence(
      SignIntoEligibleAccount(),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WaitForPromo(
          feature_engagement::kIPHContextualTasksPinnedIconChangedFeature),
      CheckView(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting,
          [](user_education::HelpBubbleView* bubble) {
            return bubble->arrow();
          },
          views::BubbleBorder::TOP_RIGHT),
      PressButton(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPinnedIconChangedPromoInteractiveTest,
                       PRE_DoesNotShowPromoWhenNewlyPinnedInSuperGArm) {
  RunTestSequence(
      SignIntoEligibleAccount(), Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      CheckPromoRequested(
          feature_engagement::kIPHContextualTasksPinnedIconChangedFeature,
          false));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPinnedIconChangedPromoInteractiveTest,
                       DoesNotShowPromoWhenNewlyPinnedInSuperGArm) {
  RunTestSequence(
      SignIntoEligibleAccount(),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      CheckPromoRequested(
          feature_engagement::kIPHContextualTasksPinnedIconChangedFeature,
          false));
}

class ContextualTasksPinnedButtonActiveIndicatorCircleHighlightInteractiveTest
    : public ContextualTasksEphemeralButtonPinningV2InteractiveTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        ContextualTasksEphemeralButtonPinningV2InteractiveTest::
            GetEnabledFeatures();
    enabled_features.push_back(
        {contextual_tasks::kContextualTasksPinnedButtonActiveIndicator,
         {{"ContextualTasksPinnedButtonActiveIndicatorCircleHighlight",
           "true"}}});
    return enabled_features;
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksPinnedButtonActiveIndicatorCircleHighlightInteractiveTest,
    ShowsCircleHighlightOnlyWhenPanelClosedAndRefreshesOnHighlight) {
  raw_ptr<PinnedActionToolbarButton> pinned_button = nullptr;

  auto VerifyCircleHighlight = [&](bool expected_active) {
    ASSERT_NE(pinned_button, nullptr);
    actions::ActionItem* const action_item =
        actions::ActionManager::Get().FindAction(
            kActionSidePanelShowContextualTasks,
            BrowserActions::From(browser())->root_action_item());
    ASSERT_NE(action_item, nullptr);
    EXPECT_EQ(action_item->GetProperty(kActionItemEngagedIndicatorStyleKey),
              ActionItemEngagedIndicatorStyle::kCircleHighlight);
    EXPECT_EQ(action_item->GetProperty(kActionItemUnderlineIndicatorKey),
              expected_active);
    ASSERT_NE(pinned_button->GetStatusIndicatorForTesting(), nullptr);
    EXPECT_FALSE(pinned_button->GetStatusIndicatorForTesting()->GetVisible());
    EXPECT_EQ(pinned_button->GetBackground() != nullptr, expected_active);
  };

  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      AddInstrumentedTab(kSecondTab, GetTestURL()),
      SelectTab(kTabStripElementId, 0),
      Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      CheckView(kPinnedToolbarActionShowSidePanelContextualTasksElementId,
                [&pinned_button](PinnedActionToolbarButton* b) {
                  pinned_button = b;
                  return true;
                }),
      // Zero state: no circle highlight, and close side panel action is not
      // registered.
      Do([&]() {
        VerifyCircleHighlight(false);
        EXPECT_EQ(actions::ActionManager::Get().FindAction(
                      kActionContextualTasksCloseSidePanel,
                      BrowserActions::From(browser())->root_action_item()),
                  nullptr);
      }),
      // Create an ongoing task on Tab 0 and open/close the side panel: while
      // the side panel is open, `ShouldShowPinnedButtonActiveIndicator()`
      // returns false in circle highlight mode; closing the side panel shows
      // the circle highlight and hides the underline.
      CreateTaskForTab(0),
      SimulateOpeningContextualTaskSidePanel(),
      Do([&]() { VerifyCircleHighlight(false); }),
      SimulateClosingContextualTaskSidePanel(),
      Do([&]() { VerifyCircleHighlight(true); }),
      // Dynamically switching the engaged indicator style updates the status
      // indicator and background immediately.
      Do([&]() {
        pinned_button->SetEngagedIndicatorStyle(
            ActionItemEngagedIndicatorStyle::kUnderline);
        EXPECT_TRUE(
            pinned_button->GetStatusIndicatorForTesting()->GetVisible());
        EXPECT_EQ(pinned_button->GetBackground(), nullptr);
        pinned_button->SetEngagedIndicatorStyle(
            ActionItemEngagedIndicatorStyle::kCircleHighlight);
        EXPECT_FALSE(
            pinned_button->GetStatusIndicatorForTesting()->GetVisible());
        EXPECT_NE(pinned_button->GetBackground(), nullptr);
      }),
      // Highlighting the button hides the circle background; resetting restores
      // it.
      Do([&]() {
        pinned_button->AddHighlight();
        EXPECT_EQ(pinned_button->GetBackground(), nullptr);
        pinned_button->ResetHighlight();
        EXPECT_NE(pinned_button->GetBackground(), nullptr);
      }),
      // Switching to Tab 1 (no ongoing task) clears the circle highlight.
      SelectTab(kTabStripElementId, 1),
      Do([&]() { VerifyCircleHighlight(false); }),
      // Switching back to Tab 0 restores the circle highlight.
      SelectTab(kTabStripElementId, 0),
      Do([&]() { VerifyCircleHighlight(true); }),
      // Disassociating the task clears the circle highlight.
      RemoveTaskFromTab(0),
      Do([&]() { VerifyCircleHighlight(false); }));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksPinnedButtonActiveIndicatorCircleHighlightInteractiveTest,
    OtherPinnedActionKeepsUnderlineIndicator) {
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      Do([&]() {
        auto* const model =
            PinnedToolbarActionsModel::Get(browser()->GetProfile());
        model->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
        model->UpdatePinnedState(kActionSidePanelShowBookmarks, true);
      }),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      WaitForShow(kPinnedToolbarActionShowSidePanelBookmarksElementId),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      SimulateClosingContextualTaskSidePanel(),
      Do([&]() {
        actions::ActionItem* const bookmarks_action =
            actions::ActionManager::Get().FindAction(
                kActionSidePanelShowBookmarks,
                BrowserActions::From(browser())->root_action_item());
        ASSERT_NE(bookmarks_action, nullptr);
        bookmarks_action->SetProperty(kActionItemUnderlineIndicatorKey, true);
      }),
      CheckView(
          kPinnedToolbarActionShowSidePanelBookmarksElementId,
          [](PinnedActionToolbarButton* bookmarks_button) {
            return bookmarks_button->GetStatusIndicatorForTesting() !=
                       nullptr &&
                   bookmarks_button->GetStatusIndicatorForTesting()
                       ->GetVisible() &&
                   bookmarks_button->GetBackground() == nullptr;
          },
          true),
      CheckView(
          kPinnedToolbarActionShowSidePanelContextualTasksElementId,
          [](PinnedActionToolbarButton* contextual_tasks_button) {
            return contextual_tasks_button->GetStatusIndicatorForTesting() !=
                       nullptr &&
                   !contextual_tasks_button->GetStatusIndicatorForTesting()
                        ->GetVisible() &&
                   contextual_tasks_button->GetBackground() != nullptr;
          },
          true));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksPinnedButtonActiveIndicatorCircleHighlightInteractiveTest,
    EphemeralButtonShowsCircleHighlightOnlyWhenPanelClosed) {
  raw_ptr<ContextualTasksButton> ephemeral_button = nullptr;
  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, false);
      }),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      CheckView(
          kContextualTasksEphemeralToolbarButtonElementId,
          [&ephemeral_button](ContextualTasksButton* button) {
            ephemeral_button = button;
            return button->GetStatusIndicatorForTesting() != nullptr &&
                   !button->GetStatusIndicatorForTesting()->GetVisible() &&
                   !button->ShouldShowCircleHighlight() &&
                   button->GetBackground() == nullptr &&
                   button->GetDropShadowLayerForTesting() == nullptr;
          },
          true),
      SimulateClosingContextualTaskSidePanel(),
      WaitForShow(kContextualTasksEphemeralToolbarButtonElementId),
      CheckView(
          kContextualTasksEphemeralToolbarButtonElementId,
          [](ContextualTasksButton* button) {
            return button->GetStatusIndicatorForTesting() != nullptr &&
                   !button->GetStatusIndicatorForTesting()->GetVisible() &&
                   button->ShouldShowCircleHighlight() &&
                   button->GetBackground() != nullptr &&
                   button->GetDropShadowLayerForTesting() == nullptr;
          },
          true),
      RemoveTaskFromTab(0),
      WaitForHide(kContextualTasksEphemeralToolbarButtonElementId),
      Do([&]() {
        ASSERT_NE(ephemeral_button, nullptr);
        EXPECT_FALSE(ephemeral_button->ShouldShowCircleHighlight());
      }));
}

class ContextualTasksNonBrandedEntryPointCircleHighlightInteractiveTest
    : public ContextualTasksEphemeralButtonInteractiveTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {
        {contextual_tasks::kContextualTasks,
         {{"ContextualTasksExpandButtonOptions", "toolbar-close-button"}}},
        {contextual_tasks::kContextualTasksHideCloseButtonInVerticalTabs, {}},
        {contextual_tasks::kEnableContextualTasksPinButtonInToolbar, {}},
        {contextual_tasks::kContextualTasksPinnedButtonActiveIndicator,
         {{"ContextualTasksPinnedButtonActiveIndicatorCircleHighlight",
           "true"}}}};
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return {contextual_tasks::kContextualTasksEphemeralBrandedEntryPoint};
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksNonBrandedEntryPointCircleHighlightInteractiveTest,
    TracksPanelOpenStateAndUpdatesActiveIndicator) {
  raw_ptr<PinnedActionToolbarButton> pinned_button = nullptr;

  RunTestSequence(
      SignIntoEligibleAccount(), InstrumentTab(kFirstTab),
      Do([&]() {
        PinnedToolbarActionsModel::Get(browser()->GetProfile())
            ->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
      }),
      WaitForShow(kPinnedToolbarActionShowSidePanelContextualTasksElementId),
      CheckView(kPinnedToolbarActionShowSidePanelContextualTasksElementId,
                [&pinned_button](PinnedActionToolbarButton* b) {
                  pinned_button = b;
                  return true;
                }),
      CreateTaskForTab(0), SimulateOpeningContextualTaskSidePanel(),
      Do([&]() {
        auto* const controller =
            ContextualTasksEphemeralButtonController::From(browser());
        ASSERT_NE(controller, nullptr);
        actions::ActionItem* const action_item =
            actions::ActionManager::Get().FindAction(
                kActionSidePanelShowContextualTasks,
                BrowserActions::From(browser())->root_action_item());
        ASSERT_NE(action_item, nullptr);

        // Panel is initially open after SimulateOpeningContextualTaskSidePanel:
        // circle highlight is hidden while open.
        EXPECT_FALSE(
            action_item->GetProperty(kActionItemUnderlineIndicatorKey));
        EXPECT_EQ(pinned_button->GetBackground(), nullptr);

        // OnEntryWillHide: panel is hiding with results, marking the tab
        // eligible and showing the circle highlight.
        pinned_button->ResetHighlight();
        controller->OnEntryWillHide(
            /*entry=*/nullptr, SidePanelEntryHideReason::kSidePanelClosed);
        EXPECT_TRUE(
            action_item->GetProperty(kActionItemUnderlineIndicatorKey));
        EXPECT_NE(pinned_button->GetBackground(), nullptr);

        // OnEntryShown: panel is open, circle highlight is hidden.
        controller->OnEntryShown(/*entry=*/nullptr);
        EXPECT_FALSE(
            action_item->GetProperty(kActionItemUnderlineIndicatorKey));
        EXPECT_EQ(pinned_button->GetBackground(), nullptr);

        // OnEntryWillHide: panel is hiding, circle highlight is shown.
        controller->OnEntryWillHide(
            /*entry=*/nullptr, SidePanelEntryHideReason::kSidePanelClosed);
        EXPECT_TRUE(
            action_item->GetProperty(kActionItemUnderlineIndicatorKey));
        EXPECT_NE(pinned_button->GetBackground(), nullptr);

        // OnEntryHideCancelled: panel remains open, circle highlight is hidden.
        controller->OnEntryHideCancelled(/*entry=*/nullptr);
        EXPECT_FALSE(
            action_item->GetProperty(kActionItemUnderlineIndicatorKey));
        EXPECT_EQ(pinned_button->GetBackground(), nullptr);

        // OnEntryWillHide (backgrounded) followed by OnEntryHidden: circle
        // highlight is shown.
        controller->OnEntryWillHide(/*entry=*/nullptr,
                                    SidePanelEntryHideReason::kBackgrounded);
        EXPECT_TRUE(
            action_item->GetProperty(kActionItemUnderlineIndicatorKey));
        EXPECT_NE(pinned_button->GetBackground(), nullptr);

        controller->OnEntryHidden(/*entry=*/nullptr);
        EXPECT_TRUE(
            action_item->GetProperty(kActionItemUnderlineIndicatorKey));
        EXPECT_NE(pinned_button->GetBackground(), nullptr);
      }));
}
