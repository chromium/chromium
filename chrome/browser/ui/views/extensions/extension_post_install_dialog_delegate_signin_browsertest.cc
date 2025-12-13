// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/sync/account_extension_tracker.h"
#include "chrome/browser/extensions/sync/extension_sync_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/extensions/extension_install_ui_desktop.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_delegate.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/extensions/extension_post_install_dialog_delegate.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/service/local_data_description.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

// Tests sign in behavior from the extension installed bubble.
class ExtensionPostInstallDialogDelegateSignInBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  ExtensionPostInstallDialogDelegateSignInBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{syncer::kUnoPhase2FollowUp});
  }
  ~ExtensionPostInstallDialogDelegateSignInBrowserTest() override = default;

 protected:
  extensions::AccountExtensionTracker* account_extension_tracker() {
    return extensions::AccountExtensionTracker::Get(profile());
  }

  extensions::AccountExtensionTracker::AccountExtensionType
  GetAccountExtensionType(const extensions::ExtensionId& id) {
    return account_extension_tracker()->GetAccountExtensionType(id);
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile());
  }

  views::Widget* ShowBubble(
      scoped_refptr<const extensions::Extension> extension) {
    views::Widget::Widgets old_widgets =
        views::test::WidgetTest::GetAllWidgets();

    ExtensionInstallUIDesktop::ShowBubble(extension, browser(), profile(),
                                          SkBitmap());

    views::Widget::Widgets new_widgets =
        views::test::WidgetTest::GetAllWidgets();
    views::Widget::Widgets added_widgets;
    std::set_difference(new_widgets.begin(), new_widgets.end(),
                        old_widgets.begin(), old_widgets.end(),
                        std::inserter(added_widgets, added_widgets.begin()));

    DCHECK_EQ(1u, added_widgets.size());
    return *added_widgets.begin();
  }

  scoped_refptr<const extensions::Extension> LoadPackedExtension(
      const std::string& path) {
    extensions::ChromeTestExtensionLoader extension_loader(profile());
    extension_loader.set_pack_extension(true);
    return extension_loader.LoadExtension(test_data_dir_.AppendASCII(path));
  }

  // Initiates a sign in flow from the bubble promo shown for the provided
  // `extension`.
  void InitiateSignInFromExtensionPromo(
      scoped_refptr<const extensions::Extension> extension,
      const AccountInfo& account_info = AccountInfo()) {
    views::Widget* bubble_view_widget = ShowBubble(extension);
    ASSERT_TRUE(bubble_view_widget);
    ASSERT_TRUE(bubble_view_widget->widget_delegate());

    // The sign in promo should be shown for a syncable extension.
    EXPECT_TRUE(signin::ShouldShowExtensionSignInPromo(*browser()->profile(),
                                                       *extension));

    // Initiate a sign in from the promo.
    BubbleSignInPromoDelegate delegate(
        *browser()->tab_strip_model()->GetActiveWebContents(),
        signin_metrics::AccessPoint::kExtensionInstallBubble,
        syncer::LocalDataItemModel::DataId(extension->id()));
    delegate.OnSignIn(account_info);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that by default, signing in from the extension installed bubble will
// sign the user into sync.
IN_PROC_BROWSER_TEST_F(ExtensionPostInstallDialogDelegateSignInBrowserTest,
                       BubbleSignsIntoSync) {
  // The default browser created for tests start with one tab open on
  // about:blank.  The sign-in page is a singleton that will replace this tab.
  // This function replaces about:blank with another URL so that the sign in
  // page goes into a new tab.
  ShowSingletonTabOverwritingNTP(browser(), GURL("chrome:version"),
                                 NavigateParams::IGNORE_AND_NAVIGATE);

  // Load a syncable extension.
  auto extension = LoadPackedExtension("simple_with_file");
  ASSERT_TRUE(extension);

  views::Widget* bubble_view_widget = ShowBubble(extension);
  ASSERT_TRUE(bubble_view_widget);
  ASSERT_TRUE(bubble_view_widget->widget_delegate());

  // The sync promo should be shown for a syncable extension.
  EXPECT_TRUE(
      signin::ShouldShowExtensionSyncPromo(*browser()->profile(), *extension));

  // Simulate a user signing in from the promo. This should open up a new tab
  // with the sign in page.
  int starting_tab_count = browser()->tab_strip_model()->count();
  BubbleSignInPromoDelegate delegate(
      *browser()->tab_strip_model()->GetActiveWebContents(),
      signin_metrics::AccessPoint::kExtensionInstallBubble,
      syncer::LocalDataItemModel::DataId(extension->id()));
  delegate.OnSignIn(AccountInfo());

  EXPECT_EQ(starting_tab_count + 1, browser()->tab_strip_model()->count());
}

// Test that users can perform an explicit sign in through the extension
// installed promo in transport mode.
IN_PROC_BROWSER_TEST_F(ExtensionPostInstallDialogDelegateSignInBrowserTest,
                       BubbleExplicitSignin) {
  // Load three extensions.
  auto old_extension = LoadPackedExtension("simple_with_file");
  ASSERT_TRUE(old_extension);

  auto old_extension_2 = LoadPackedExtension("simple_with_host");
  ASSERT_TRUE(old_extension_2);

  auto new_extension = LoadPackedExtension("simple_with_icon");
  ASSERT_TRUE(new_extension);

  // Override the single thread task runner for the remainder of this test so
  // that the clock can be advanced manually. This is done after installing the
  // extensions since ScopedMockTimeMessageLoopTaskRunner does not work with any
  // RunLoops.
  base::ScopedMockTimeMessageLoopTaskRunner mock_time_task_runner;

  // Initiate a sign in for the old extensions.
  InitiateSignInFromExtensionPromo(old_extension);
  InitiateSignInFromExtensionPromo(old_extension_2);

  // Advance the clock past the maximum delay to simulate the user not
  // completing the sign in flow in time.
  mock_time_task_runner->FastForwardBy(
      extensions::AccountExtensionTracker::kMaxSigninFromExtensionBubbleDelay *
      2);

  // After some time has passed, initiate a sign in for the `new_extension`.
  InitiateSignInFromExtensionPromo(new_extension);

  // Simulate a sign in from the extensions bubble to finish what was initiated
  // above.
  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::kExtensionInstallBubble)
          .Build("testy@mctestface.com"));
  ASSERT_TRUE(SigninPrefs(*profile()->GetPrefs())
                  .GetExtensionsExplicitBrowserSignin(account_info.gaia));

  // Check that the user is now signed in for the browser in transport mode and
  // syncing for extensions is enabled.
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

  EXPECT_TRUE(extensions::sync_util::IsSyncingExtensionsEnabled(profile()));

  // Due to the long delay, the old extensions should not be promoted to an
  // account extension after sign in finishes.
  EXPECT_EQ(extensions::AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(old_extension->id()));
  EXPECT_EQ(extensions::AccountExtensionTracker::AccountExtensionType::kLocal,
            GetAccountExtensionType(old_extension_2->id()));

  // Since the clock has not advanced in this test since `new_extension` was
  // installed, it should be promoted to an account extension after sign in\
  // finishes.
  EXPECT_EQ(extensions::AccountExtensionTracker::AccountExtensionType::
                kAccountInstalledSignedIn,
            GetAccountExtensionType(new_extension->id()));
}

// Test that if the user is already signed in on the web, signing in via
// extension installed promo should still promote the extension to an account
// extension.
// This tests the fix for crbug.com/400522723
IN_PROC_BROWSER_TEST_F(ExtensionPostInstallDialogDelegateSignInBrowserTest,
                       BubbleExplicitSigninWithAccount) {
  auto extension = LoadPackedExtension("simple_with_file");
  ASSERT_TRUE(extension);

  // Simulate a sign in from the web.
  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder()
          .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
          .Build("testy@mctestface.com"));

  // The user should only be signed in the web but not the browser itself, and
  // extensions should not be syncing.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_FALSE(extensions::sync_util::IsSyncingExtensionsEnabled(profile()));

  // Now simulate signing into chrome via the extension promo for the given
  // `extension`.
  InitiateSignInFromExtensionPromo(extension, account_info);

  // The user should be signed in to the browser.
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // The extension should immediately be promoted to an account extension.
  EXPECT_EQ(extensions::AccountExtensionTracker::AccountExtensionType::
                kAccountInstalledSignedIn,
            GetAccountExtensionType(extension->id()));

  // This should be recorded as an extension explicit sign in.
  EXPECT_TRUE(SigninPrefs(*profile()->GetPrefs())
                  .GetExtensionsExplicitBrowserSignin(account_info.gaia));
  EXPECT_TRUE(extensions::sync_util::IsSyncingExtensionsEnabled(profile()));
}
