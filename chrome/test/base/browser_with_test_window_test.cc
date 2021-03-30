// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/browser_with_test_window_test.h"

#include "base/location.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "ui/base/page_transition_types.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "components/constrained_window/constrained_window_views.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "content/public/browser/context_factory.h"
#endif
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_chrome_service_delegate.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#endif

using content::NavigationController;
using content::RenderFrameHost;
using content::RenderFrameHostTester;
using content::WebContents;

BrowserWithTestWindowTest::~BrowserWithTestWindowTest() {}

void BrowserWithTestWindowTest::SetUp() {
  testing::Test::SetUp();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash_test_helper_.SetUp();
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosChromeServiceImpl::DisableCrosapiForTests();
  lacros_chrome_service_ =
      std::make_unique<chromeos::LacrosChromeServiceImpl>(nullptr);
  tablet_state_ = std::make_unique<chromeos::TabletState>();
#endif

  // This must be created after |ash_test_helper_| is set up so that it doesn't
  // create a DeviceDataManager.
  rvh_test_enabler_ = std::make_unique<content::RenderViewHostTestEnabler>();

#if defined(TOOLKIT_VIEWS)
  SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());
#endif

  profile_manager_ = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager_->SetUp());

  // Subclasses can provide their own Profile.
  profile_ = CreateProfile();
  // Subclasses can provide their own test BrowserWindow. If they return NULL
  // then Browser will create a production BrowserWindow and the subclass is
  // responsible for cleaning it up (usually by NativeWidget destruction).
  window_ = CreateBrowserWindow();

  browser_ =
      CreateBrowser(profile(), browser_type_, hosted_app_, window_.get());
}

void BrowserWithTestWindowTest::TearDown() {
  // Some tests end up posting tasks to the DB thread that must be completed
  // before the profile can be destroyed and the test safely shut down.
  base::RunLoop().RunUntilIdle();

  // Close the browser tabs and destroy the browser and window instances.
  if (browser_)
    browser_->tab_strip_model()->CloseAllTabs();
  browser_.reset();
  window_.reset();

#if defined(TOOLKIT_VIEWS)
  constrained_window::SetConstrainedWindowViewsClient(nullptr);
#endif

  profile_manager_->DeleteAllTestingProfiles();
  profile_ = nullptr;

  // Depends on LocalState owned by |profile_manager_|.
  if (SystemNetworkContextManager::GetInstance()) {
    SystemNetworkContextManager::DeleteInstance();
  }

  profile_manager_.reset();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  tablet_state_.reset();
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If initialized, the KioskAppManager will register an observer to
  // CrosSettings and will need to be destroyed before it. Having it destroyed
  // as part of the teardown will avoid unexpected test failures.
  ash::KioskAppManager::Shutdown();

  test_views_delegate_.reset();
  ash_test_helper_.TearDown();
#elif defined(TOOLKIT_VIEWS)
  views_test_helper_.reset();
#endif

  testing::Test::TearDown();

  // A Task is leaked if we don't destroy everything, then run all pending
  // tasks. This includes backend tasks which could otherwise be affected by the
  // deletion of the temp dir.
  task_environment_->RunUntilIdle();
}

gfx::NativeWindow BrowserWithTestWindowTest::GetContext() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ash_test_helper_.GetContext();
#elif defined(TOOLKIT_VIEWS)
  return views_test_helper_->GetContext();
#else
  return nullptr;
#endif
}

void BrowserWithTestWindowTest::AddTab(Browser* browser, const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_TYPED);
  params.tabstrip_index = 0;
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  CommitPendingLoad(&params.navigated_or_inserted_contents->GetController());
}

void BrowserWithTestWindowTest::CommitPendingLoad(
  NavigationController* controller) {
  if (!controller->GetPendingEntry())
    return;  // Nothing to commit.

  RenderFrameHostTester::CommitPendingLoad(controller);
}

void BrowserWithTestWindowTest::NavigateAndCommit(
    NavigationController* controller,
    const GURL& url) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      controller->GetWebContents(), url);
}

void BrowserWithTestWindowTest::NavigateAndCommitActiveTab(const GURL& url) {
  NavigateAndCommit(&browser()->tab_strip_model()->GetActiveWebContents()->
                        GetController(),
                    url);
}

void BrowserWithTestWindowTest::NavigateAndCommitActiveTabWithTitle(
    Browser* navigating_browser,
    const GURL& url,
    const std::u16string& title) {
  WebContents* contents =
      navigating_browser->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &contents->GetController();
  NavigateAndCommit(controller, url);
  contents->UpdateTitleForEntry(controller->GetActiveEntry(), title);
}

TestingProfile* BrowserWithTestWindowTest::CreateProfile() {
  return profile_manager_->CreateTestingProfile(
      "testing_profile", nullptr, std::u16string(), 0, std::string(),
      GetTestingFactories());
}

TestingProfile::TestingFactories
BrowserWithTestWindowTest::GetTestingFactories() {
  return {};
}

std::unique_ptr<BrowserWindow>
BrowserWithTestWindowTest::CreateBrowserWindow() {
  return std::make_unique<TestBrowserWindow>();
}

std::unique_ptr<Browser> BrowserWithTestWindowTest::CreateBrowser(
    Profile* profile,
    Browser::Type browser_type,
    bool hosted_app,
    BrowserWindow* browser_window) {
  Browser::CreateParams params(profile, true);
  if (hosted_app) {
    params = Browser::CreateParams::CreateForApp(
        "Test", true /* trusted_source */, gfx::Rect(), profile, true);
  } else if (browser_type == Browser::TYPE_DEVTOOLS) {
    params = Browser::CreateParams::CreateForDevTools(profile);
  } else {
    params.type = browser_type;
  }
  params.window = browser_window;
  return std::unique_ptr<Browser>(Browser::Create(params));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
ash::ScopedCrosSettingsTestHelper*
BrowserWithTestWindowTest::GetCrosSettingsHelper() {
  return &cros_settings_test_helper_;
}

chromeos::StubInstallAttributes*
BrowserWithTestWindowTest::GetInstallAttributes() {
  return GetCrosSettingsHelper()->InstallAttributes();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

BrowserWithTestWindowTest::BrowserWithTestWindowTest(
    std::unique_ptr<content::BrowserTaskEnvironment> task_environment,
    Browser::Type browser_type,
    bool hosted_app)
    : task_environment_(std::move(task_environment)),
      browser_type_(browser_type),
      hosted_app_(hosted_app) {}
