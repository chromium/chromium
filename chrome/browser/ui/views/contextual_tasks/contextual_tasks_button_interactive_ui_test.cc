// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_button.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/common/pref_names.h"
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
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"

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
                                    /*is_off_the_record=*/false) {}

  ~TestingAimEligibilityService() override = default;

  bool IsAimEligible() const override { return true; }
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

  auto SignIntoEligibleAccount() {
    return Do([&]() {
      AccountInfo primary_account_info =
          identity_test_env()->MakePrimaryAccountAvailable(
              "primary@example.com", signin::ConsentLevel::kSignin);
    });
  }

  auto ClearPrimaryAccount() {
    return Do([&]() { identity_test_env()->ClearPrimaryAccount(); });
  }

 private:
  base::CallbackListSubscription subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

class ContextualTasksButtonInteractiveTest
    : public ContextualTasksButtonInteractiveTestBase {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageActionsMigration, {}},
         {contextual_tasks::kContextualTasks,
          {{"ContextualTasksEntryPoint", "toolbar-permanent"}}},
         {features::kTabbedBrowserUseNewLayout, {}}},
        {});
    InteractiveBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksButtonInteractiveTest,
                       ShowContextualTaskButton) {
  RunTestSequence(
      SignIntoEligibleAccount(),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton),
      Check([&] {
        return GetPrefService()->GetBoolean(prefs::kPinContextualTaskButton);
      }),
      Do([&] {
        GetPrefService()->SetBoolean(prefs::kPinContextualTaskButton, false);
      }),
      WaitForHide(ContextualTasksButton::kContextualTasksToolbarButton),
      Do([&] {
        GetPrefService()->SetBoolean(prefs::kPinContextualTaskButton, true);
      }),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksButtonInteractiveTest,
                       ToggleToolbarHeightSidePanel) {
  RunTestSequence(
      SignIntoEligibleAccount(),
      EnsurePresent(ContextualTasksButton::kContextualTasksToolbarButton),
      PressButton(ContextualTasksButton::kContextualTasksToolbarButton),
      WaitForShow(kSidePanelElementId),
      PressButton(ContextualTasksButton::kContextualTasksToolbarButton),
      WaitForHide(kSidePanelElementId));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksButtonInteractiveTest,
                       VisiblityUpdatesOnAimPolicyChange) {
  RunTestSequence(
      SignIntoEligibleAccount(),
      EnsurePresent(ContextualTasksButton::kContextualTasksToolbarButton),
      Do([&]() {
        PrefService* const pref_service = browser()->profile()->GetPrefs();
        pref_service->SetInteger(omnibox::kAIModeSettings, 1);
      }),
      WaitForHide(ContextualTasksButton::kContextualTasksToolbarButton),
      Do([&]() {
        PrefService* const pref_service = browser()->profile()->GetPrefs();
        pref_service->SetInteger(omnibox::kAIModeSettings, 0);
      }),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton));
}

#if !BUILDFLAG(IS_CHROMEOS)
// CrOS identity is tied to the OS logged in state so it doesn't make sense to
// have the browser open without an identity.
IN_PROC_BROWSER_TEST_F(ContextualTasksButtonInteractiveTest,
                       BrowserIdentityTriggersVisibility) {
  RunTestSequence(
      EnsureNotPresent(ContextualTasksButton::kContextualTasksToolbarButton),
      SignIntoEligibleAccount(),
      WaitForShow(ContextualTasksButton::kContextualTasksToolbarButton),
      ClearPrimaryAccount(),
      WaitForHide(ContextualTasksButton::kContextualTasksToolbarButton));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

class ContextualTasksEphemeralButtonInteractiveTest
    : public ContextualTasksButtonInteractiveTestBase {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasks,
          {{"ContextualTasksEntryPoint", "toolbar-revisit"}}},
         {features::kTabbedBrowserUseNewLayout, {}}},
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
      contextual_tasks::ContextualTasksSidePanelCoordinator::From(browser())
          ->Show();
    });
  }

  // Simulate closing the contextual task side panel as if you clicked on the
  // close button in the contextual task side panel.
  auto SimulateClosingContextualTaskSidePanel() {
    return Do([&] {
      contextual_tasks::ContextualTasksSidePanelCoordinator::From(browser())
          ->Close();
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
                       ButtonVisibilityIsPreservedAsSidePanelToggles) {
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
      PressButton(ContextualTasksButton::kContextualTasksToolbarButton),
      WaitForHide(kSidePanelElementId),
      EnsurePresent(ContextualTasksButton::kContextualTasksToolbarButton));
}
