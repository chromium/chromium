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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

// Browser tests that consider ReduceUserAgentAndroidVersionDeviceModel feature
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
        /*enabled_features=*/{blink::features::kReduceUserAgentMinorVersion,
                              blink::features::
                                  kReduceUserAgentAndroidVersionDeviceModel},
        /*disabled_features=*/{});
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReduceUserAgentAndroidPlatformBrowserTest,
                       NavigatorPlatform) {
  // We should reduce android navigator.platform in phase 6.
  EXPECT_EQ("Linux armv81",
            content::EvalJs(GetActiveWebContents(), "navigator.platform"));
}

// Browser tests that consider ReduceUserAgentAndroidVersionDeviceModel feature
// disabled.
class DisableFeatureReduceUserAgentAndroidPlatformBrowserTest
    : public ReduceUserAgentAndroidPlatformBrowserTest {
 public:
  // Copy the implementation of NavigatorID::platform().
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
        /*enabled_features=*/{}, /*disabled_features=*/{
            blink::features::kReduceUserAgentAndroidVersionDeviceModel});
  }
};

IN_PROC_BROWSER_TEST_F(DisableFeatureReduceUserAgentAndroidPlatformBrowserTest,
                       NavigatorPlatform) {
  // We should not reduce android navigator.platform when feature is disabled.
  EXPECT_EQ(GetPlatform(),
            content::EvalJs(GetActiveWebContents(), "navigator.platform"));
}

// Verifying User-Agent and User-Agent Client Hints on Android.
class AndroidUserAgentClientHintsBrowserTest
    : public ReduceUserAgentAndroidPlatformBrowserTest {
 public:
  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::EvalJsResult getUserAgentClientHints() {
    const std::string script = R"(
      (async () => {
        const hints = await navigator.userAgentData.getHighEntropyValues([
          "architecture", "bitness", "formFactors", "fullVersionList",
          "model", "platformVersion", "uaFullVersion", "wow64"
        ]);
        return hints;
      })()
    )";

    content::EvalJsResult result =
        content::EvalJs(GetActiveWebContents(), script);
    EXPECT_TRUE(result.is_dict());
    return result;
  }
};

IN_PROC_BROWSER_TEST_F(AndroidUserAgentClientHintsBrowserTest,
                       UserAgentAndUserAgentClientHints) {
  GURL url = embedded_test_server()->GetURL("/simple_page.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), url));

  // 1. Verify the User Agent string.
  std::string user_agent =
      content::EvalJs(GetActiveWebContents(), "navigator.userAgent")
          .ExtractString();
  EXPECT_THAT(user_agent, testing::HasSubstr("Linux; Android 10; K"));

  // 2. Verify User Agent Client Hints.
  content::EvalJsResult result = getUserAgentClientHints();
  const base::Value::Dict& hints = result.ExtractDict();

  // Verify low-entropy client hints.
  EXPECT_FALSE(hints.Find("brands")->GetList().empty());
  EXPECT_EQ(hints.Find("platform")->GetString(), "Android");
  EXPECT_TRUE(hints.Find("mobile")->is_bool());

  // Verify high-entropy client hints.
  EXPECT_EQ(hints.Find("architecture")->GetString(), "");
  EXPECT_EQ(hints.Find("bitness")->GetString(), "");
  EXPECT_FALSE(hints.Find("fullVersionList")->GetList().empty());
  EXPECT_FALSE(hints.Find("model")->GetString().empty());
  EXPECT_FALSE(hints.Find("platformVersion")->GetString().empty());
  EXPECT_FALSE(hints.Find("uaFullVersion")->GetString().empty());
  EXPECT_FALSE(hints.Find("wow64")->GetBool());

  std::vector<std::string> form_factors;
  for (const auto& form_factor : hints.Find("formFactors")->GetList()) {
    if (form_factor.is_string()) {
      form_factors.push_back(form_factor.GetString());
    }
  }
  EXPECT_EQ(form_factors.size(), 1u);
  // Expects the default "Desktop" or "Mobile" form factor, it depends on
  // whether it turns on the command lined switch `kUseMobileUserAgent`.
  EXPECT_THAT(form_factors[0],
              testing::AnyOf(testing::Eq("Mobile"), testing::Eq("Desktop")));
}
