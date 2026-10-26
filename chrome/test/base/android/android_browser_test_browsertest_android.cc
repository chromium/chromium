// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/test/test_network_connection_tracker.h"

IN_PROC_BROWSER_TEST_F(AndroidBrowserTest, Smoke) {
  ASSERT_EQ(TabModelList::models().size(), 1u);

  // Grab a tab an navigate its contents
  const TabModel* tab_model = TabModelList::models()[0];
  EXPECT_TRUE(content::NavigateToURL(tab_model->GetActiveWebContents(),
                                     GURL("about:blank")));
}

// Verifies that state is preserved between PRE_ stages on Android.
// Historically PRE_ was not supported, which is why there's an explicit test
// for this behavior.
IN_PROC_BROWSER_TEST_F(AndroidBrowserTest, PRE_PRE_StatePreserved) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  Profile* profile = GetProfile();
  ASSERT_FALSE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_PRE_DIR"))));
  ASSERT_FALSE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_DIR"))));

  ASSERT_TRUE(base::CreateDirectory(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_PRE_DIR"))));
  ASSERT_TRUE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_PRE_DIR"))));
}

IN_PROC_BROWSER_TEST_F(AndroidBrowserTest, PRE_StatePreserved) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  Profile* profile = GetProfile();
  ASSERT_TRUE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_PRE_DIR"))));
  ASSERT_FALSE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_DIR"))));

  ASSERT_TRUE(base::CreateDirectory(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_DIR"))));
}

IN_PROC_BROWSER_TEST_F(AndroidBrowserTest, StatePreserved) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  Profile* profile = GetProfile();
  ASSERT_TRUE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_PRE_DIR"))));
  ASSERT_TRUE(base::DirectoryExists(
      profile->GetPath().Append(FILE_PATH_LITERAL("PRE_DIR"))));
}

IN_PROC_BROWSER_TEST_F(AndroidBrowserTest, NetworkIsReportedOnline) {
  auto type = net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE;
  const bool answered_synchronously =
      content::GetNetworkConnectionTracker()->GetConnectionType(
          &type, base::DoNothing());

  EXPECT_TRUE(answered_synchronously);
  EXPECT_NE(net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE, type);
}

class AndroidBrowserTestWithRealNetworkConnectionTracker
    : public AndroidBrowserTest {
 public:
  AndroidBrowserTestWithRealNetworkConnectionTracker() {
    set_use_test_network_connection_tracker(false);
  }
};

IN_PROC_BROWSER_TEST_F(AndroidBrowserTestWithRealNetworkConnectionTracker,
                       UsesRealTracker) {
  EXPECT_FALSE(network::TestNetworkConnectionTracker::HasInstance());
}
