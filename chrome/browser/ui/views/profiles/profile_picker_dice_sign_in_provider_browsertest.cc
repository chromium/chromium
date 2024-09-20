// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_dice_sign_in_provider.h"

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_deletion_observer.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::HasSubstr;
using testing::Not;

namespace {

const char kExpectedSigninBaseUrl[] =
    "https://accounts.google.com/signin/chrome/sync";

class MockHost : public ProfilePickerWebContentsHost {
 public:
  MOCK_METHOD(void,
              ShowScreen,
              (content::WebContents * contents,
               const GURL& url,
               base::OnceClosure navigation_finished_closure));
  MOCK_METHOD(void,
              ShowScreenInPickerContents,
              (const GURL& url, base::OnceClosure navigation_finished_closure));
  MOCK_METHOD(bool, ShouldUseDarkColors, (), (const));
  MOCK_METHOD(content::WebContents*, GetPickerContents, (), (const));
  MOCK_METHOD(void, SetNativeToolbarVisible, (bool visible));
  MOCK_METHOD(SkColor, GetPreferredBackgroundColor, (), (const));
  MOCK_METHOD(content::WebContentsDelegate*, GetWebContentsDelegate, ());
  MOCK_METHOD(web_modal::WebContentsModalDialogHost*,
              GetWebContentsModalDialogHost,
              ());
  MOCK_METHOD(void, Reset, (StepSwitchFinishedCallback callback));
  MOCK_METHOD(void,
              ShowForceSigninErrorDialog,
              (const ForceSigninUIError& error, bool success));
};

Profile* GetContentsProfile(content::WebContents* contents) {
  return Profile::FromBrowserContext(contents->GetBrowserContext());
}

}  // namespace

class ProfilePickerDiceSignInProviderBrowserTest : public InProcessBrowserTest {
 public:
  ProfilePickerDiceSignInProviderBrowserTest() = default;
  ~ProfilePickerDiceSignInProviderBrowserTest() override = default;

  testing::NiceMock<MockHost>* host() { return &host_; }

 private:
  testing::NiceMock<MockHost> host_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProfilePickerDiceSignInProviderBrowserTest,
                       SwitchToSignInThenExit) {
  ProfileDeletionObserver observer;
  base::FilePath provider_profile_path;
  base::RunLoop switch_finished_loop;
  base::MockCallback<ProfilePickerDiceSignInProvider::SignedInCallback>
      signin_finished_callback;

  // Sign-in is exited, the callback should never run.
  EXPECT_CALL(signin_finished_callback, Run(_, _, _)).Times(0);

  {
    ProfilePickerDiceSignInProvider provider{
        host(), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN};

    EXPECT_CALL(*host(), ShowScreen(_, _, _))
        .WillOnce([&](content::WebContents* contents, const GURL& url,
                      base::OnceClosure callback) {
          provider_profile_path = GetContentsProfile(contents)->GetPath();
          EXPECT_FALSE(provider_profile_path.empty());
          EXPECT_NE(browser()->profile()->GetPath(), provider_profile_path);

          EXPECT_TRUE(url.spec().starts_with(kExpectedSigninBaseUrl));
          EXPECT_THAT(url.query(), HasSubstr("flow=promo"));

          std::move(callback).Run();
        });

    provider.SwitchToSignIn(
        base::IgnoreArgs<bool>(switch_finished_loop.QuitClosure()),
        signin_finished_callback.Get());

    switch_finished_loop.Run();
  }

  // On exit, the profile it created should be scheduled for deletion since it
  // has no more keep alives.
  observer.Wait();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(provider_profile_path);
  EXPECT_EQ(entry, nullptr);
}

IN_PROC_BROWSER_TEST_F(ProfilePickerDiceSignInProviderBrowserTest,
                       SwitchToSignInThenExit_ForFirstRun) {
  base::FilePath provider_profile_path;
  base::RunLoop switch_finished_loop;
  base::MockCallback<ProfilePickerDiceSignInProvider::SignedInCallback>
      signin_finished_callback;

  // Sign-in is exited, the callback should never run.
  EXPECT_CALL(signin_finished_callback, Run(_, _, _)).Times(0);

  {
    ProfilePickerDiceSignInProvider provider{
        host(), signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE,
        browser()->profile()->GetPath()};

    EXPECT_CALL(*host(), ShowScreen(_, _, _))
        .WillOnce([&](content::WebContents* contents, const GURL& url,
                      base::OnceClosure callback) {
          provider_profile_path = GetContentsProfile(contents)->GetPath();
          EXPECT_FALSE(provider_profile_path.empty());
          EXPECT_EQ(browser()->profile()->GetPath(), provider_profile_path);

          EXPECT_TRUE(url.spec().starts_with(kExpectedSigninBaseUrl));
          EXPECT_THAT(url.query(), HasSubstr("flow=promo"));

          std::move(callback).Run();
        });

    provider.SwitchToSignIn(
        base::IgnoreArgs<bool>(switch_finished_loop.QuitClosure()),
        signin_finished_callback.Get());

    switch_finished_loop.Run();
  }

  // Since a profile has been passed in, the provider should not delete it.
  EXPECT_FALSE(IsProfileDirectoryMarkedForDeletion(provider_profile_path));
}
