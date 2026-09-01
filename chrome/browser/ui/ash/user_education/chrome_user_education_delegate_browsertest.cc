// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/user_education/chrome_user_education_delegate.h"

#include <memory>
#include <string>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/user_education/user_education_util.h"
#include "base/callback_list.h"
#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/system_web_apps/system_web_app_type.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/tutorial/tutorial_description.h"
#include "components/user_education/common/tutorial/tutorial_registry.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/display/types/display_constants.h"

namespace {
// Element identifiers.
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kElementId);
constexpr auto kElementContext =
    ui::ElementContext::CreateFakeContextForTesting(1);
}  // namespace

// ChromeUserEducationDelegateBrowserTest --------------------------------------

// Base class for browser tests of the `ChromeUserEducationDelegate`.
class ChromeUserEducationDelegateBrowserTest
    : public ash::SystemWebAppBrowserTestBase {
 public:
  // Returns a pointer to the `delegate_` instance under test.
  ash::UserEducationDelegate* delegate() { return delegate_.get(); }

  Profile* profile() { return browser()->GetProfile(); }

  // Returns the `AccountId` for the primary `profile()`.
  const AccountId& account_id() {
    return ash::BrowserContextHelper::Get()
        ->GetUserByBrowserContext(profile())
        ->GetAccountId();
  }

 private:
  // ash::SystemWebAppBrowserTestBase:
  void SetUpOnMainThread() override {
    ash::SystemWebAppBrowserTestBase::SetUpOnMainThread();

    // Instantiate the `delegate_` after
    // `ash::SystemWebAppBrowserTestBase::SetUpOnMainThread()` so that the
    // browser process has fully initialized.
    delegate_ = std::make_unique<ChromeUserEducationDelegate>();
  }

  // The delegate instance under test.
  std::unique_ptr<ChromeUserEducationDelegate> delegate_;
};

// Tests -----------------------------------------------------------------------

// Verifies that `LaunchSystemWebAppAsync()` is working as intended.
IN_PROC_BROWSER_TEST_F(ChromeUserEducationDelegateBrowserTest,
                       LaunchSystemWebAppAsync) {
  // Wait for Explore app installation.
  WaitForTestSystemAppInstall();
  base::HistogramTester histogram_tester;

  // Attempt to launch Explore app.
  delegate()->LaunchSystemWebAppAsync(account_id(), ash::SystemWebAppType::HELP,
                                      apps::LaunchSource::kFromWelcomeTour,
                                      display::kDefaultDisplayId);

  // Expect Explore app to launch asynchronously.
  EXPECT_TRUE(base::test::RunUntil([]() {
    BrowserWindowInterface* const browser =
        GetLastActiveBrowserWindowInterfaceWithAnyProfile();
    return browser &&
           web_app::AppBrowserController::IsForWebApp(browser, ash::kHelpAppId);
  }));
  histogram_tester.ExpectUniqueSample("Apps.DefaultAppLaunch.FromWelcomeTour",
                                      apps::DefaultAppName::kHelpApp, 1);
}

// Verifies that `GetElementIdentifierForAppId()` is working as intended.
IN_PROC_BROWSER_TEST_F(ChromeUserEducationDelegateBrowserTest,
                       GetElementIdentifierForAppId) {
  using AppIdWithElementIdentifier =
      std::pair<const char*, std::optional<ui::ElementIdentifier>>;

  const std::array<AppIdWithElementIdentifier, 4u> kAppIdsWithElementIds = {
      {{ash::kHelpAppId, ash::kExploreAppElementId},
       {ash::kOsSettingsAppId, ash::kSettingsAppElementId},
       {"unknown", std::nullopt},
       {"", std::nullopt}}};

  for (const auto& [app_id, element_id] : kAppIdsWithElementIds) {
    EXPECT_EQ(delegate()->GetElementIdentifierForAppId(app_id), element_id);
  }
}

// Verifies `RegisterTutorial()` registers a tutorial with the browser registry
// and that `IsTutorialRegistered()` accurately reflects browser registry state.
IN_PROC_BROWSER_TEST_F(ChromeUserEducationDelegateBrowserTest,
                       RegisterTutorial) {
  static constexpr auto kTutorialIds =
      base::EnumSet<ash::TutorialId, ash::TutorialId::kMinValue,
                    ash::TutorialId::kMaxValue>::All();

  const ash::TutorialId tutorial_id = ash::TutorialId::kTest1;
  const auto tutorial_id_str = ash::user_education_util::ToString(tutorial_id);

  const user_education::TutorialRegistry& tutorial_registry =
      UserEducationServiceFactory::GetForBrowserContext(profile())
          ->tutorial_registry();

  // Initially there should be no tutorial registered.
  EXPECT_FALSE(tutorial_registry.IsTutorialRegistered(tutorial_id_str));
  for (ash::TutorialId candidate_tutorial_id : kTutorialIds) {
    EXPECT_FALSE(
        delegate()->IsTutorialRegistered(account_id(), candidate_tutorial_id));
  }

  // Attempt to register a tutorial.
  delegate()->RegisterTutorial(account_id(), tutorial_id,
                               user_education::TutorialDescription());

  // Confirm tutorial registration.
  EXPECT_TRUE(tutorial_registry.IsTutorialRegistered(tutorial_id_str));
  for (ash::TutorialId candidate_tutorial_id : kTutorialIds) {
    EXPECT_EQ(
        delegate()->IsTutorialRegistered(account_id(), candidate_tutorial_id),
        candidate_tutorial_id == tutorial_id);
  }
}

// Verifies `StartTutorial()` starts a tutorial with the browser service, and
// `AbortTutorial()` will abort the tutorial.
IN_PROC_BROWSER_TEST_F(ChromeUserEducationDelegateBrowserTest,
                       StartAndAbortTutorial) {
  // Create a test element.
  ui::test::TestElement test_element(kElementId, kElementContext);

  // Create a tutorial description.
  user_education::TutorialDescription tutorial_description;
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kElementId)
          .SetBubbleBodyText(IDS_OK));

  // Register the tutorial.
  delegate()->RegisterTutorial(account_id(), ash::TutorialId::kTest1,
                               std::move(tutorial_description));

  // Verify the tutorial is not running.
  user_education::TutorialService* tutorial_service =
      UserEducationServiceFactory::GetForBrowserContext(profile())
          ->tutorial_service();
  EXPECT_FALSE(delegate()->IsRunningTutorial(account_id()));

  // Attempt to start the tutorial.
  UNCALLED_MOCK_CALLBACK(base::OnceClosure, aborted_callback);
  delegate()->StartTutorial(
      account_id(), ash::TutorialId::kTest1, kElementContext,
      /*completed_callback=*/base::BindLambdaForTesting([]() { FAIL(); }),
      aborted_callback.Get());

  // Confirm the tutorial is running.
  EXPECT_TRUE(tutorial_service->IsRunningTutorial());

  // Verify the running tutorial's ID.
  EXPECT_TRUE(
      delegate()->IsRunningTutorial(account_id(), ash::TutorialId::kTest1));

  // Abort the tutorial and expect the callback to be called.
  EXPECT_CALL_IN_SCOPE(aborted_callback, Run,
                       delegate()->AbortTutorial(account_id()));
  EXPECT_FALSE(delegate()->IsRunningTutorial(account_id()));
}

// Verifies that `AbortTutorial()` will only abort the tutorial associated with
// the given id, when it is given.
IN_PROC_BROWSER_TEST_F(ChromeUserEducationDelegateBrowserTest,
                       AbortSpecificTutorial) {
  const auto kTestTutorialIdString =
      ash::user_education_util::ToString(ash::TutorialId::kTest1);

  // Create a test element.
  ui::test::TestElement test_element(kElementId, kElementContext);

  // Create a tutorial description.
  user_education::TutorialDescription tutorial_description;
  tutorial_description.steps.emplace_back(
      user_education::TutorialDescription::BubbleStep(kElementId)
          .SetBubbleBodyText(IDS_OK));

  // Register the tutorial.
  delegate()->RegisterTutorial(account_id(), ash::TutorialId::kTest1,
                               std::move(tutorial_description));

  // Verify the tutorial is not running.
  user_education::TutorialService* tutorial_service =
      UserEducationServiceFactory::GetForBrowserContext(profile())
          ->tutorial_service();
  EXPECT_FALSE(tutorial_service->IsRunningTutorial(kTestTutorialIdString));

  // Attempt to start the tutorial.
  delegate()->StartTutorial(account_id(), ash::TutorialId::kTest1,
                            kElementContext,
                            /*completed_callback=*/base::DoNothing(),
                            /*aborted_callback=*/base::DoNothing());

  // Confirm the tutorial is running.
  EXPECT_TRUE(tutorial_service->IsRunningTutorial(kTestTutorialIdString));

  // Abort the tutorial with the incorrect id, and expect the tutorial to still
  // be running.
  delegate()->AbortTutorial(account_id(), ash::TutorialId::kTest2);
  EXPECT_TRUE(tutorial_service->IsRunningTutorial(kTestTutorialIdString));

  // Abort the tutorial with the correct id, and expect no tutorial to be
  // running.
  delegate()->AbortTutorial(account_id(), ash::TutorialId::kTest1);
  EXPECT_FALSE(tutorial_service->IsRunningTutorial());
}

namespace {

// Aliases.
using ::testing::NiceMock;

// Mocks -----------------------------------------------------------------------

class MockAppListSyncableService : public app_list::AppListSyncableService {
 public:
  explicit MockAppListSyncableService(Profile* profile)
      : app_list::AppListSyncableService(profile) {}

  // app_list::AppListSyncableService:
  MOCK_METHOD(void,
              OnFirstSync,
              (base::OnceCallback<void(bool was_first_sync_ever)> callback),
              (override));
};

}  // namespace

// Base class for tests of the `ChromeUserEducationDelegate` concerned with
// new users, parameterized by whether the first app list sync in the session
// was the first sync ever across all ChromeOS devices and sessions for the
// given user.
class ChromeUserEducationDelegateNewUserBrowserTest
    : public ChromeUserEducationDelegateBrowserTest,
      public ::testing::WithParamInterface</*was_first_sync_ever=*/bool> {
 public:
  // Returns the event to signal when the first app list sync in the session has
  // been completed.
  base::OneShotEvent& on_first_sync() { return on_first_sync_; }

  // Returns whether the first app list sync in the session was the first sync
  // ever across all ChromeOS devices and sessions for the given user, based on
  // test parameterization.
  bool was_first_sync_ever() const { return GetParam(); }

 private:
  void SetUp() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ChromeUserEducationDelegateNewUserBrowserTest::
                    SetMockTestingFactory,
                base::Unretained(this)));
    ChromeUserEducationDelegateBrowserTest::SetUp();
  }

  void SetMockTestingFactory(content::BrowserContext* context) {
    app_list::AppListSyncableServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            &ChromeUserEducationDelegateNewUserBrowserTest::CreateMockService,
            base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateMockService(
      content::BrowserContext* browser_context) {
    auto app_list_syncable_service =
        std::make_unique<NiceMock<MockAppListSyncableService>>(
            Profile::FromBrowserContext(browser_context));

    // Mock `app_list::AppListSyncableService::OnFirstSync()` so that
    // it runs callbacks to inform them if the first app list sync in
    // the session was the first sync ever across all ChromeOS
    // devices and sessions for the given user, based on test
    // parameterization. Callbacks should only run once signaled that
    // the first app list sync in the session has been completed.
    ON_CALL(*app_list_syncable_service, OnFirstSync)
        .WillByDefault(
            [&](base::OnceCallback<void(bool was_first_sync_ever)> callback) {
              on_first_sync_.Post(
                  FROM_HERE,
                  base::BindOnce(std::move(callback), was_first_sync_ever()));
            });

    return app_list_syncable_service;
  }

  // The event to signal when the first app list sync in the session has been
  // completed.
  base::OneShotEvent on_first_sync_;
  base::CallbackListSubscription subscription_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeUserEducationDelegateNewUserBrowserTest,
                         /*was_first_sync_ever=*/::testing::Bool());

// Tests -----------------------------------------------------------------------

// Verifies that `IsNewUser()` is working as intended.
IN_PROC_BROWSER_TEST_P(ChromeUserEducationDelegateNewUserBrowserTest,
                       IsNewUser) {
  // Until the first app list sync in the session has been completed, it is
  // not known whether a given user can be considered new.
  EXPECT_EQ(delegate()->IsNewUser(account_id()), std::nullopt);

  // Signal that the first app list sync in the session has been completed.
  on_first_sync().Signal();

  // Once the first app list sync in the session has been completed, a task will
  // be posted to the `delegate()` which will cache whether the given user can
  // be considered new.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return delegate()->IsNewUser(account_id()) == was_first_sync_ever();
  }));
}
