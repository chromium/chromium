// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_glic_flow_controller.h"

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "chrome/test/base/profile_waiter.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfilePickerGlicFlowControllerBrowserTest : public InProcessBrowserTest {
 public:
  ProfilePickerGlicFlowControllerBrowserTest() = default;
  ~ProfilePickerGlicFlowControllerBrowserTest() override = default;

  testing::NiceMock<MockProfilePickerWebContentsHost>* host() { return &host_; }

 private:
  testing::NiceMock<MockProfilePickerWebContentsHost> host_;
};

IN_PROC_BROWSER_TEST_F(ProfilePickerGlicFlowControllerBrowserTest,
                       InitController) {
  EXPECT_CALL(*host(), ShowScreenInPickerContents(
                           GURL("chrome://profile-picker/?glic"), testing::_));

  ProfilePickerGlicFlowController controller(
      host(), ClearHostClosure(base::DoNothing()), base::DoNothing());
  controller.Init();
}

IN_PROC_BROWSER_TEST_F(ProfilePickerGlicFlowControllerBrowserTest,
                       PickProfileWithProfileNotLoaded) {
  // Create a new Profile and destroy it immediately so that it is not loaded.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile* new_profile =
      &profiles::testing::CreateProfileSync(profile_manager, new_profile_path);
  ProfileDestructionWaiter profile_destruction_waiter(new_profile);
  Browser* new_browser = CreateBrowser(new_profile);
  CloseBrowserSynchronously(new_browser);
  profile_destruction_waiter.Wait();

  base::MockCallback<base::OnceClosure> clear_host_callback;
  EXPECT_CALL(clear_host_callback, Run());

  base::MockCallback<base::OnceCallback<void(Profile*)>>
      picked_profile_callback;
  ProfileWaiter profile_waiter;
  // Make sure that the loaded Profile is valid and corresponds to the newly
  // created one.
  EXPECT_CALL(picked_profile_callback, Run(testing::_))
      .WillOnce([&new_profile_path, &profile_manager](Profile* profile) {
        ASSERT_TRUE(profile);
        EXPECT_EQ(profile->GetPath(), new_profile_path);
        EXPECT_FALSE(profile_manager->HasKeepAliveForTesting(
            profile, ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow));
        EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
            profile, ProfileKeepAliveOrigin::kWaitingForGlicView));
      });

  ProfilePickerGlicFlowController controller(
      host(), ClearHostClosure(clear_host_callback.Get()),
      picked_profile_callback.Get());
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  EXPECT_CALL(mock_callback, Run(true));
  controller.PickProfile(new_profile_path, ProfilePicker::ProfilePickingArgs(),
                         mock_callback.Get());

  Profile* loaded_profile = profile_waiter.WaitForProfileAdded();
  signin::WaitForRefreshTokensLoaded(
      IdentityManagerFactory::GetForProfile(loaded_profile));
}

// TODO(crbug.com/404425678): Re-enable failing test on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PickProfileWithCurrentProfile \
  DISABLED_PickProfileWithCurrentProfile
#else
#define MAYBE_PickProfileWithCurrentProfile PickProfileWithCurrentProfile
#endif
IN_PROC_BROWSER_TEST_F(ProfilePickerGlicFlowControllerBrowserTest,
                       MAYBE_PickProfileWithCurrentProfile) {
  base::MockCallback<base::OnceClosure> clear_host_callback;
  EXPECT_CALL(clear_host_callback, Run());

  base::MockCallback<base::OnceCallback<void(Profile*)>>
      picked_profile_callback;
  // Return the currently active profile right away if it is already loaded.
  EXPECT_CALL(picked_profile_callback, Run(browser()->profile()));

  ProfilePickerGlicFlowController controller(
      host(), ClearHostClosure(clear_host_callback.Get()),
      picked_profile_callback.Get());
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  EXPECT_CALL(mock_callback, Run(true));
  controller.PickProfile(browser()->profile()->GetPath(),
                         ProfilePicker::ProfilePickingArgs(),
                         mock_callback.Get());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerGlicFlowControllerBrowserTest,
                       PickProfileWithNonExistingProfile) {
  base::MockCallback<base::OnceClosure> clear_host_callback;
  EXPECT_CALL(clear_host_callback, Run());

  base::MockCallback<base::OnceCallback<void(Profile*)>>
      picked_profile_callback;
  // Callback returns a nullptr profile for non existing profiles.
  EXPECT_CALL(picked_profile_callback, Run(nullptr));

  ProfilePickerGlicFlowController controller(
      host(), ClearHostClosure(clear_host_callback.Get()),
      picked_profile_callback.Get());
  // Next profile directly is guaranteed not to be tied to an existing profile
  // as it was not created yet.
  base::FilePath non_profile_file_path =
      g_browser_process->profile_manager()->GenerateNextProfileDirectoryPath();
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  EXPECT_CALL(mock_callback, Run(false));
  controller.PickProfile(non_profile_file_path,
                         ProfilePicker::ProfilePickingArgs(),
                         mock_callback.Get());
}

IN_PROC_BROWSER_TEST_F(ProfilePickerGlicFlowControllerBrowserTest,
                       ClearingControllerShouldCallTheInputCallbacks) {
  base::MockCallback<base::OnceClosure> clear_host_callback;
  EXPECT_CALL(clear_host_callback, Run());

  base::MockCallback<base::OnceCallback<void(Profile*)>>
      picked_profile_callback;
  // Callback returns a nullptr profile if no profile was selected.
  EXPECT_CALL(picked_profile_callback, Run(nullptr));

  {
    ProfilePickerGlicFlowController controller(
        host(), ClearHostClosure(clear_host_callback.Get()),
        picked_profile_callback.Get());
  }
  // Controller is destroyed without any profile selection.
}
