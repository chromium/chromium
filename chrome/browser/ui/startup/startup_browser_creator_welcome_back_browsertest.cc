// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_tab_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {
typedef absl::optional<policy::PolicyLevel> PolicyVariant;
}

class StartupBrowserCreatorWelcomeBackTest : public InProcessBrowserTest {
 protected:
  StartupBrowserCreatorWelcomeBackTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    profile_ = browser()->profile();

    // Keep the browser process and Profile running when all browsers are
    // closed.
    scoped_keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED);
    scoped_profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        profile_, ProfileKeepAliveOrigin::kBrowserWindow);
    // Close the browser opened by InProcessBrowserTest.
    CloseBrowserSynchronously(browser());
    ASSERT_EQ(0U, BrowserList::GetInstance()->size());
  }

  void StartBrowser(PolicyVariant variant) {
    browser_creator_.set_welcome_back_page(true);

    if (variant) {
      policy::PolicyMap values;
      values.Set(policy::key::kRestoreOnStartup, variant.value(),
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 base::Value(4), nullptr);
      base::Value::List url_list;
      url_list.Append("http://managed.site.com/");
      values.Set(policy::key::kRestoreOnStartupURLs, variant.value(),
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                 base::Value(std::move(url_list)), nullptr);
      provider_.UpdateChromePolicy(values);
    }

    ASSERT_TRUE(browser_creator_.Start(
        base::CommandLine(base::CommandLine::NO_PROGRAM), base::FilePath(),
        {raw_ptr(profile_.get()), StartupProfileMode::kBrowserWindow},
        g_browser_process->profile_manager()->GetLastOpenedProfiles()));
    ASSERT_EQ(1U, BrowserList::GetInstance()->size());
  }

  void ExpectUrlInBrowserAtPosition(const GURL& url, int tab_index) {
    Browser* browser = BrowserList::GetInstance()->get(0);
    TabStripModel* tab_strip = browser->tab_strip_model();
    EXPECT_EQ(url, tab_strip->GetWebContentsAt(tab_index)->GetVisibleURL());
  }

  void TearDownOnMainThread() override {
    scoped_profile_keep_alive_.reset();
    scoped_keep_alive_.reset();
  }

 private:
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
  std::unique_ptr<ScopedKeepAlive> scoped_keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> scoped_profile_keep_alive_;
  StartupBrowserCreator browser_creator_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorWelcomeBackTest,
                       WelcomeBackStandardNoPolicy) {
  ASSERT_NO_FATAL_FAILURE(StartBrowser(PolicyVariant()));
  ExpectUrlInBrowserAtPosition(StartupTabProviderImpl::GetWelcomePageUrl(false),
                               0);
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorWelcomeBackTest,
                       WelcomeBackStandardMandatoryPolicy) {
  ASSERT_NO_FATAL_FAILURE(
      StartBrowser(PolicyVariant(policy::POLICY_LEVEL_MANDATORY)));
  ExpectUrlInBrowserAtPosition(GURL("http://managed.site.com/"), 0);
}

IN_PROC_BROWSER_TEST_F(StartupBrowserCreatorWelcomeBackTest,
                       WelcomeBackStandardRecommendedPolicy) {
  ASSERT_NO_FATAL_FAILURE(
      StartBrowser(PolicyVariant(policy::POLICY_LEVEL_RECOMMENDED)));
  ExpectUrlInBrowserAtPosition(GURL("http://managed.site.com/"), 0);
}
