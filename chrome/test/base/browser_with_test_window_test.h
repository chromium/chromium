// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_BROWSER_WITH_TEST_WINDOW_TEST_H_
#define CHROME_TEST_BASE_BROWSER_WITH_TEST_WINDOW_TEST_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/test/views/chrome_test_views_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_views_delegate.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#else
#include "ui/views/test/scoped_views_test_helper.h"
#endif
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

class GURL;

namespace chromeos {
class ScopedLacrosServiceTestHelper;
}  // namespace chromeos

namespace content {
class NavigationController;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace crosapi {
class CrosapiManager;
}
#endif

class TestingProfileManager;

// WARNING WARNING WARNING WARNING
// Do not use this class. See docs/chrome_browser_design_principles.md for
// details. Either write a browser test which provides both a "class Browser"
// and a "class BrowserView" or a unit test which requires neither.
// Historically, features were written that take a "class Browser" as an input
// parameter. "class Browser" cannot be stubbed/faked/mocked, and this class was
// written as a workaround to create a "class Browser" without a "class
// BrowserView". This cannot happen in production code and thus results in test
// logic leaking into production code. New features should not take "class
// Browser" as input, and should instead perform dependency injection.
//
// Base class for browser based unit tests. BrowserWithTestWindowTest creates a
// Browser with a TestingProfile and TestBrowserWindow. To add a tab use
// AddTab. For example, the following adds a tab and navigates to two URLs:
//
//   // Add a new tab and navigate it. This will be at index 0.
//   // WARNING: this creates a real WebContents. If you want to add a test
//   // WebContents create it directly and insert it into the TabStripModel.
//   AddTab(browser(), GURL("http://foo/1"));
//   WebContents* contents = browser()->tab_strip_model()->GetWebContentsAt(0);
//
//   // Navigate somewhere else.
//   GURL url2("http://foo/2");
//   NavigateAndCommit(contents, url2);
//
//   // This is equivalent to the above, and lets you test pending navigations.
//   browser()->OpenURL(OpenURLParams(
//       GURL("http://foo/2"), GURL(), WindowOpenDisposition::CURRENT_TAB,
//       ui::PAGE_TRANSITION_TYPED, false), /*navigation_handle_callback=*/{});
//   CommitPendingLoad(&contents->GetController());
//
// Subclasses must invoke BrowserWithTestWindowTest::SetUp as it is responsible
// for creating the various objects of this class.
class BrowserWithTestWindowTest : public testing::Test, public ProfileObserver {
 public:
  // Trait which requests construction of a hosted app.
  struct HostedApp {};

  struct ValidTraits {
    explicit ValidTraits(content::BrowserTaskEnvironment::ValidTraits);
    explicit ValidTraits(HostedApp);
    explicit ValidTraits(Browser::Type);

    // TODO(alexclarke): Make content::BrowserTaskEnvironment::ValidTraits
    // imply this.
    explicit ValidTraits(base::test::TaskEnvironment::ValidTraits);
  };

  // Creates a BrowserWithTestWindowTest with zero or more traits. By default
  // the initial window will be a tabbed browser created on the native desktop,
  // which is not a hosted app.
  template <typename... TaskEnvironmentTraits>
    requires base::trait_helpers::AreValidTraits<ValidTraits,
                                                 TaskEnvironmentTraits...>
  NOINLINE explicit BrowserWithTestWindowTest(TaskEnvironmentTraits... traits)
      : BrowserWithTestWindowTest(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::trait_helpers::Exclude<HostedApp, Browser::Type>::Filter(
                    traits)...),
            base::trait_helpers::GetEnum<Browser::Type, Browser::TYPE_NORMAL>(
                traits...),
            base::trait_helpers::HasTrait<HostedApp,
                                          TaskEnvironmentTraits...>()) {}

  BrowserWithTestWindowTest(const BrowserWithTestWindowTest&) = delete;
  BrowserWithTestWindowTest& operator=(const BrowserWithTestWindowTest&) =
      delete;

  ~BrowserWithTestWindowTest() override;

  void SetUp() override;
  void TearDown() override;

  // Set up process for `profile_manager_`. If a `profile_manager` is supplied,
  // it will be set as the underlying profile manager that `profile_manager_`
  // uses, aka `profile_manager_->profile_manager()`. This can only be called
  // before ::SetUp().
  void SetUpProfileManager(
      const base::FilePath& profiles_path = base::FilePath(),
      std::unique_ptr<ProfileManager> profile_manager = nullptr);

 protected:
  BrowserWindow* window() const { return window_.get(); }

  Browser* browser() const { return browser_.get(); }
  void set_browser(Browser* browser) { browser_.reset(browser); }
  std::unique_ptr<Browser> release_browser() { return std::move(browser_); }

  TestingProfile* profile() const { return profile_; }

  TestingProfile* GetProfile() { return profile_; }

  TestingProfileManager* profile_manager() { return profile_manager_.get(); }

  content::BrowserTaskEnvironment* task_environment() {
    return task_environment_.get();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  std::unique_ptr<BrowserWindow> release_browser_window() {
    return std::move(window_);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::AshTestHelper* ash_test_helper() { return &ash_test_helper_; }
  user_manager::FakeUserManager* user_manager() { return user_manager_.Get(); }
#endif

  // The context to help determine desktop type when creating new Widgets.
  gfx::NativeWindow GetContext();

  // Adds a tab to |browser| with the given URL and commits the load.
  // This is a convenience function. The new tab will be added at index 0.
  // WARNING: this creates a real WebContents. If you want to add a test
  // WebContents create it directly and insert it into the TabStripModel.
  void AddTab(Browser* browser, const GURL& url);

  // Commits the pending load on the given controller. It will keep the
  // URL of the pending load. If there is no pending load, this does nothing.
  void CommitPendingLoad(content::NavigationController* controller);

  // Creates a pending navigation on the given WebContents to the given URL with
  // the default parameters and the commits the load with a page ID one larger
  // than any seen. This emulates what happens on a new navigation.
  void NavigateAndCommit(content::WebContents* web_contents, const GURL& url);

  // Navigates the current tab. This is a wrapper around NavigateAndCommit.
  void NavigateAndCommitActiveTab(const GURL& url);

  // Set the |title| of the current tab.
  void NavigateAndCommitActiveTabWithTitle(Browser* browser,
                                           const GURL& url,
                                           const std::u16string& title);

  // Sets the focused frame to the main frame of the active WebContents, for
  // tests that rely on the focused frame not being null.
  void FocusMainFrameOfActiveWebContents();

  // Returns the profile name used for the profile created in SetUp() by
  // default.
  // Subclasses can override to change the profile name.
  virtual std::string GetDefaultProfileName();

  // Creates the profile used by this test. The caller doesn't own the return
  // value.
  virtual TestingProfile* CreateProfile(const std::string& profile_name);

  // Deletes the specified profile.
  // If `profile_name` is the one returned from GetDefaultProfileName(),
  // because this instance creates Browser for the profile in SetUp() and keeps
  // it in a member, the Browser instance will also be destroyed to avoid leak.
  virtual void DeleteProfile(const std::string& profile_name);

  // Returns a vector of testing factories to be used when creating the profile.
  // This is only used by CreateProfile(), and will be irrelevant if that
  // method is overridden.
  virtual TestingProfile::TestingFactories GetTestingFactories();

  // Creates the BrowserWindow used by this test. Subclasses can provide their
  // own test BrowserWindow. If the provided BrowserWindow is null then Browser
  // will create a production BrowserWindow and the subclass is responsible for
  // cleaning it up (usually by NativeWidget destruction).
  virtual std::unique_ptr<BrowserWindow> CreateBrowserWindow();

  // Creates the browser given |profile|, |browser_type|, |hosted_app|, and
  // |browser_window|.
  virtual std::unique_ptr<Browser> CreateBrowser(Profile* profile,
                                                 Browser::Type browser_type,
                                                 bool hosted_app,
                                                 BrowserWindow* browser_window);

#if defined(TOOLKIT_VIEWS)
  views::TestViewsDelegate* test_views_delegate() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    return test_views_delegate_.get();
#else
    return views_test_helper_->test_views_delegate();
#endif
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Logs in an User as `email`.
  virtual void LogIn(const std::string& email);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Handles the post-process for the newly created Profile.
  // Expected to be called on customizing CreateProfile for ash.
  virtual void OnUserProfileCreated(const std::string& email, Profile* profile);

  // Switches the active user to the one specified by the email.
  virtual void SwitchActiveUser(const std::string& email);

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  ash::ScopedCrosSettingsTestHelper* GetCrosSettingsHelper();
  ash::StubInstallAttributes* GetInstallAttributes();
#endif

 private:
  // The template constructor has to be in the header but it delegates to this
  // constructor to initialize all other members out-of-line.
  BrowserWithTestWindowTest(
      std::unique_ptr<content::BrowserTaskEnvironment> task_environment,
      Browser::Type browser_type,
      bool hosted_app);

  // We need to create a MessageLoop, otherwise a bunch of things fails.
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<chromeos::ScopedLacrosServiceTestHelper>
      lacros_service_test_helper_;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      user_manager_;
  std::vector<
      std::unique_ptr<base::ScopedObservation<Profile, ProfileObserver>>>
      profile_observations_;
  std::unique_ptr<crosapi::CrosapiManager> manager_;
  std::unique_ptr<ash::KioskChromeAppManager> kiosk_chrome_app_manager_;
#endif

  raw_ptr<TestingProfile, AcrossTasksDanglingUntriaged> profile_ = nullptr;

  // test_url_loader_factory_ is declared before profile_manager_
  // to guarantee it outlives any profiles that might use it.
  network::TestURLLoaderFactory test_url_loader_factory_;

  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<BrowserWindow> window_;  // Usually a TestBrowserWindow.
  std::unique_ptr<Browser> browser_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::AshTestHelper ash_test_helper_;
  std::unique_ptr<views::TestViewsDelegate> test_views_delegate_ =
      std::make_unique<ChromeTestViewsDelegate<ash::AshTestViewsDelegate>>();
#elif defined(TOOLKIT_VIEWS)
  std::unique_ptr<views::ScopedViewsTestHelper> views_test_helper_ =
      std::make_unique<views::ScopedViewsTestHelper>(
          std::make_unique<ChromeTestViewsDelegate<>>());
#endif

  // The existence of this object enables tests via RenderViewHostTester.
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;

#if BUILDFLAG(IS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  // The type of browser to create (tabbed or popup).
  const Browser::Type browser_type_;

  // Whether the browser is part of a hosted app.
  const bool hosted_app_;

  // Initialize the variations provider.
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  // Some of the UI elements in top chrome need to observe the
  // UserPerformanceTuningManager, so create and install a fake.
  performance_manager::user_tuning::TestUserPerformanceTuningManagerEnvironment
      user_performance_tuning_manager_environment_;
};

#endif  // CHROME_TEST_BASE_BROWSER_WITH_TEST_WINDOW_TEST_H_
