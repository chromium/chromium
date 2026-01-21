// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/utsname.h>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"

// Browser tests that consider kReduceUserAgentMinorVersion feature
// enabled.
class ReduceUserAgentAndroidPlatformBrowserTest : public AndroidBrowserTest {
 public:
  ReduceUserAgentAndroidPlatformBrowserTest() = default;

  void SetUp() override {
    SetupFeatures();
    AndroidBrowserTest::SetUp();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 protected:
  virtual void SetupFeatures() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kReduceUserAgentMinorVersion},
        /*disabled_features=*/{});
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReduceUserAgentAndroidPlatformBrowserTest,
                       NavigatorPlatform) {
  // navigator.platform is always "Linux armv81" on Android.
  EXPECT_EQ("Linux armv81",
            content::EvalJs(GetActiveWebContents(), "navigator.platform"));
}

// Browser tests that consider kReduceUserAgentMinorVersion feature
// disabled.
class DisableFeatureReduceUserAgentAndroidPlatformBrowserTest
    : public ReduceUserAgentAndroidPlatformBrowserTest {
 public:
  // TODO(crbug.com/469458271): We shouldn't copy the implementation here once
  // we remove kReduceUserAgentMinorVersion (since that flag now controls
  // sending the unified platform), or we can just delete this test.
  // Copy the implementation of NavigatorID::platform()
  std::string GetPlatform() {
    struct utsname osname;
    std::string platform_name;
    if (uname(&osname) >= 0) {
      base::StrAppend(&platform_name, {osname.sysname});
      if (strlen(osname.machine) != 0) {
        base::StrAppend(&platform_name, {" ", osname.machine});
      }
    }
    return platform_name;
  }

 protected:
  void SetupFeatures() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{blink::features::kReduceUserAgentMinorVersion});
  }
};

IN_PROC_BROWSER_TEST_F(DisableFeatureReduceUserAgentAndroidPlatformBrowserTest,
                       NavigatorPlatform) {
  // If kReduceUserAgentMinorVersion is disabled (which it will be for Android
  // WebView), navigator.platform() should not be "reduced" (and the
  // implementation of GetPlatform in this test will return the un-reduced
  // value).
  EXPECT_EQ(GetPlatform(),
            content::EvalJs(GetActiveWebContents(), "navigator.platform"));
}
