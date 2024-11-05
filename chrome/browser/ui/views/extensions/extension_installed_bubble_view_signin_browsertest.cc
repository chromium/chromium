// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_sync_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/extensions/extension_installed_bubble_view.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

// Tests sign in behavior from the extension installed bubble.
class ExtensionInstalledBubbleViewsSignInBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  ExtensionInstalledBubbleViewsSignInBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        syncer::kSyncEnableExtensionsInTransportMode);
  }

  ~ExtensionInstalledBubbleViewsSignInBrowserTest() override = default;

 protected:
  views::Widget* ShowBubble(
      scoped_refptr<const extensions::Extension> extension) {
    views::Widget::Widgets old_widgets =
        views::test::WidgetTest::GetAllWidgets();

    ExtensionInstallUI::ShowBubble(extension, browser(), SkBitmap());

    views::Widget::Widgets new_widgets =
        views::test::WidgetTest::GetAllWidgets();
    views::Widget::Widgets added_widgets;
    std::set_difference(new_widgets.begin(), new_widgets.end(),
                        old_widgets.begin(), old_widgets.end(),
                        std::inserter(added_widgets, added_widgets.begin()));

    DCHECK_EQ(1u, added_widgets.size());
    return *added_widgets.begin();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that by default, signing in from the extension installed bubble will
// sign the user into sync.
IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleViewsSignInBrowserTest,
                       BubbleSignsIntoSync) {
  // The default browser created for tests start with one tab open on
  // about:blank.  The sign-in page is a singleton that will replace this tab.
  // This function replaces about:blank with another URL so that the sign in
  // page goes into a new tab.
  ShowSingletonTabOverwritingNTP(browser(), GURL("chrome:version"),
                                 NavigateParams::IGNORE_AND_NAVIGATE);

  // Load a syncable extension.
  extensions::ChromeTestExtensionLoader extension_loader(profile());
  extension_loader.set_pack_extension(true);
  scoped_refptr<const extensions::Extension> extension =
      extension_loader.LoadExtension(
          test_data_dir_.AppendASCII("simple_with_file"));
  ASSERT_TRUE(extension);

  views::Widget* bubble_view_widget = ShowBubble(extension);
  ASSERT_TRUE(bubble_view_widget);

  ExtensionInstalledBubbleView* view_delegate =
      static_cast<ExtensionInstalledBubbleView*>(
          bubble_view_widget->widget_delegate());
  ASSERT_TRUE(view_delegate);

  // The sign in promo should be shown for a syncable extension.
  EXPECT_TRUE(view_delegate->model()->show_sign_in_promo());

  // Simulate a user signing in from the promo. This should open up a new tab
  // with the sign in page.
  int starting_tab_count = browser()->tab_strip_model()->count();
  view_delegate->SignInForTesting(AccountInfo());

  EXPECT_EQ(starting_tab_count + 1, browser()->tab_strip_model()->count());
}

// A variant of the above test except signing in from the promo will be an
// explicit sign in to transport mode.
class ExtensionInstalledBubbleViewsExplicitSignInBrowserTest
    : public ExtensionInstalledBubbleViewsSignInBrowserTest {
 public:
  ExtensionInstalledBubbleViewsExplicitSignInBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {switches::kExplicitBrowserSigninUIOnDesktop,
         syncer::kSyncEnableExtensionsInTransportMode},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExtensionInstalledBubbleViewsExplicitSignInBrowserTest,
                       BubbleExplicitSignin) {
  // Load a syncable extension.
  extensions::ChromeTestExtensionLoader extension_loader(profile());
  extension_loader.set_pack_extension(true);
  scoped_refptr<const extensions::Extension> extension =
      extension_loader.LoadExtension(
          test_data_dir_.AppendASCII("simple_with_file"));
  ASSERT_TRUE(extension);

  views::Widget* bubble_view_widget = ShowBubble(extension);
  ASSERT_TRUE(bubble_view_widget);

  ExtensionInstalledBubbleView* view_delegate =
      static_cast<ExtensionInstalledBubbleView*>(
          bubble_view_widget->widget_delegate());
  ASSERT_TRUE(view_delegate);

  // The sign in promo should be shown for a syncable extension.
  EXPECT_TRUE(view_delegate->model()->show_sign_in_promo());

  // Simulate a sign in to the web. This makes it so that the user can be
  // signed in directly from the promo.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());

  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager,
      signin::AccountAvailabilityOptionsBuilder()
          .WithAccessPoint(signin_metrics::AccessPoint::
                               ACCESS_POINT_EXTENSION_INSTALL_BUBBLE)
          .Build("test@gmail.com"));

  view_delegate->SignInForTesting(account_info);

  // Check that the user is now signed in for the browser in transport mode and
  // syncing for extensions is enabled.
  EXPECT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  EXPECT_FALSE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));

  EXPECT_TRUE(extensions::sync_util::IsSyncingExtensionsEnabled(profile()));
}
