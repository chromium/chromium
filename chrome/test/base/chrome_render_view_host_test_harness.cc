// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_render_view_host_test_harness.h"

#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/shell.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using content::RenderViewHostTester;
using content::RenderViewHostTestHarness;

ChromeRenderViewHostTestHarness::~ChromeRenderViewHostTestHarness() = default;

TestingProfile* ChromeRenderViewHostTestHarness::profile() {
  return static_cast<TestingProfile*>(browser_context());
}

void ChromeRenderViewHostTestHarness::SetUp() {
#if BUILDFLAG(IS_CHROMEOS)
  if (log_in_params_) {
    ash::LoginState::Initialize();
    auto* browser_process = TestingBrowserProcess::GetGlobal();
    auto delegate =
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            browser_process,
            // `this` owns the environment, which owns the delegate.
            base::BindRepeating(
                &ChromeRenderViewHostTestHarness::GetTestingFactories,
                base::Unretained(this)));
    session_delegate_ = delegate.get();
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(
            browser_process->local_state(), std::move(delegate));

    const AccountId& account_id = log_in_params_->account_id;
    switch (log_in_params_->user_type) {
      case user_manager::UserType::kRegular:
        CHECK(user_session_test_environment_->AddRegularUser(account_id));
        break;
      case user_manager::UserType::kKioskWebApp:
        CHECK(user_session_test_environment_->AddKioskWebAppUser(
            account_id.GetUserEmail()));
        break;
      default:
        NOTREACHED() << "Unsupported user type";
    }
    if (log_in_params_->is_managed || log_in_params_->is_affiliated) {
      user_manager::UserManager::Get()->SetUserPolicyStatus(
          account_id, log_in_params_->is_managed,
          log_in_params_->is_affiliated);
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  RenderViewHostTestHarness::SetUp();
}

void ChromeRenderViewHostTestHarness::TearDown() {
  RenderViewHostTestHarness::TearDown();
#if BUILDFLAG(IS_CHROMEOS)
  ash::Shell::DeleteInstance();
  session_delegate_ = nullptr;
  user_session_test_environment_.reset();
  if (log_in_params_) {
    ash::LoginState::Shutdown();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TestingProfile::TestingFactories
ChromeRenderViewHostTestHarness::GetTestingFactories() const {
  return {};
}

std::unique_ptr<TestingProfile>
ChromeRenderViewHostTestHarness::CreateTestingProfile() {
  TestingProfile::Builder builder;
  builder.AddTestingFactories(GetTestingFactories());
  return builder.Build();
}

std::unique_ptr<content::BrowserContext>
ChromeRenderViewHostTestHarness::CreateBrowserContext() {
#if BUILDFLAG(IS_CHROMEOS)
  // Log in here, where the base's SetUp() creates its context, so the profile
  // is created after the test doubles that SetUp() installs. The profile is
  // owned by the environment's TestingProfileManager and served from
  // GetBrowserContext().
  if (log_in_params_) {
    const AccountId& account_id = log_in_params_->account_id;
    user_session_test_environment_->LogIn(account_id);
    user_browser_context_ =
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            account_id);
    CHECK(user_browser_context_);
    return nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return CreateTestingProfile();
}

#if BUILDFLAG(IS_CHROMEOS)
content::BrowserContext* ChromeRenderViewHostTestHarness::GetBrowserContext() {
  if (log_in_params_) {
    CHECK(user_browser_context_);
    return user_browser_context_;
  }
  return RenderViewHostTestHarness::GetBrowserContext();
}

void ChromeRenderViewHostTestHarness::DestroyBrowserContext() {
  if (log_in_params_) {
    // Flush any pending tasks posted during rph_factory_ teardown before
    // synchronously deleting the logged-in profile.
    base::RunLoop().RunUntilIdle();
    user_browser_context_ = nullptr;
    session_delegate_->testing_profile_manager().DeleteAllTestingProfiles();
    return;
  }
  RenderViewHostTestHarness::DestroyBrowserContext();
}
#endif  // BUILDFLAG(IS_CHROMEOS)
