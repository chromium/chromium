// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_RENDER_VIEW_HOST_TEST_HARNESS_H_
#define CHROME_TEST_BASE_CHROME_RENDER_VIEW_HOST_TEST_HARNESS_H_

#include <memory>
#include <optional>
#include <utility>

#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_renderer_host.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user_type.h"  // nogncheck

namespace ash::test {
class ChromeUserSessionTestEnvironmentDelegate;
}  // namespace ash::test
#endif  // BUILDFLAG(IS_CHROMEOS)

// Wrapper around RenderViewHostTestHarness that uses a TestingProfile as
// browser context instead of a TestBrowserContext.
class ChromeRenderViewHostTestHarness
    : public content::RenderViewHostTestHarness {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  // The ChromeOS user that SetUp() logs in.
  struct LogInParams {
    AccountId account_id;
    user_manager::UserType user_type = user_manager::UserType::kRegular;
    bool is_managed = false;
    bool is_affiliated = false;
  };
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Construct a ChromeRenderViewHostTestHarness with zero or more arguments
  // passed to content::RenderViewHostTestHarness.
  template <typename... TaskEnvironmentTraits>
  explicit ChromeRenderViewHostTestHarness(TaskEnvironmentTraits&&... traits)
      : content::RenderViewHostTestHarness(
            std::forward<TaskEnvironmentTraits>(traits)...) {}

#if BUILDFLAG(IS_CHROMEOS)
  // Runs each test with the user in `log_in_params` logged in. SetUp()
  // initializes LoginState and creates a UserSessionTestEnvironment with the
  // Chrome delegate; the user logs in during InitBrowserContext(), so
  // profile() is that user's profile, built with GetTestingFactories() and
  // owned by the environment's TestingProfileManager rather than by the
  // harness. The profile is deleted in DestroyBrowserContext(); TearDown()
  // then resets the environment and LoginState.
  template <typename... TaskEnvironmentTraits>
  explicit ChromeRenderViewHostTestHarness(LogInParams log_in_params,
                                           TaskEnvironmentTraits&&... traits)
      : content::RenderViewHostTestHarness(
            std::forward<TaskEnvironmentTraits>(traits)...),
        log_in_params_(std::move(log_in_params)) {}
#endif  // BUILDFLAG(IS_CHROMEOS)

  ~ChromeRenderViewHostTestHarness() override;

  TestingProfile* profile();

 protected:
  // testing::Test
  void SetUp() override;
  void TearDown() override;

  // Returns a list of factories to use when creating the TestingProfile.
  // Can be overridden by sub-classes if needed.
  virtual TestingProfile::TestingFactories GetTestingFactories() const;

  // Creates a TestingProfile to use as the browser context. Not called on
  // ChromeOS when `LogInParams` is set; in that mode the profile is built via
  // `TestingProfileManager` using `GetTestingFactories()`.
  virtual std::unique_ptr<TestingProfile> CreateTestingProfile();

  // content::RenderViewHostTestHarness.
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() final;

#if BUILDFLAG(IS_CHROMEOS)
  // content::RenderViewHostTestHarness.
  content::BrowserContext* GetBrowserContext() override;
  void DestroyBrowserContext() override;
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
#if BUILDFLAG(IS_CHROMEOS)
  const std::optional<LogInParams> log_in_params_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<ash::test::ChromeUserSessionTestEnvironmentDelegate>
      session_delegate_ = nullptr;
  raw_ptr<content::BrowserContext> user_browser_context_ = nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

#endif  // CHROME_TEST_BASE_CHROME_RENDER_VIEW_HOST_TEST_HARNESS_H_
