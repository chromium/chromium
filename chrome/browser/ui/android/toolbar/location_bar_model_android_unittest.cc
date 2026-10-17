// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/location_bar_model_android.h"

#include "base/android/device_info.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class TestLocationBarModelAndroid : public LocationBarModelAndroid {
 public:
  TestLocationBarModelAndroid() : LocationBarModelAndroid(nullptr) {}
  ~TestLocationBarModelAndroid() override = default;

  // LocationBarModelDelegate:
  bool GetURL(GURL* url) const override {
    *url = url_;
    return true;
  }

  // LocationBarModelAndroid:
  bool IsToolbarUiRefactorEnabled() const override {
    return is_toolbar_ui_refactor_enabled_;
  }

  void SetURL(const GURL& url) { url_ = url; }
  void SetIsToolbarUiRefactorEnabled(bool enabled) {
    is_toolbar_ui_refactor_enabled_ = enabled;
  }

 private:
  GURL url_;
  bool is_toolbar_ui_refactor_enabled_ = true;
};

// Fixture for tests that depend on the desktop Android WebUI NTP, which is
// gated on the device form factor and chrome::android::kUseWebUiNtpAndroid.
class LocationBarModelAndroidWebUiNtpTest : public testing::Test {
 protected:
  void TearDown() override {
    base::android::device_info::reset_is_desktop_for_testing();
  }

  void EnableDesktopAndroidWebUiNtp() {
    base::android::device_info::set_is_desktop_for_testing(true);
    scoped_feature_list_.InitAndEnableFeature(
        chrome::android::kUseWebUiNtpAndroid);
  }

  void DisableDesktopAndroidWebUiNtp() {
    base::android::device_info::set_is_desktop_for_testing(true);
    scoped_feature_list_.InitAndDisableFeature(
        chrome::android::kUseWebUiNtpAndroid);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

TEST(LocationBarModelAndroidTest, ClassifyAndroidNativeNewTabPage) {
  TestLocationBarModelAndroid location_bar_model_android;
  location_bar_model_android.SetURL(GURL(chrome::kChromeUINativeNewTabURL));
  EXPECT_EQ(
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      location_bar_model_android.GetPageClassification(false));

  std::string ntp_with_path_and_query =
      std::string(chrome::kChromeUINativeNewTabURL) + "foopath?foo=bar";
  location_bar_model_android.SetURL(GURL(ntp_with_path_and_query));
  EXPECT_EQ(
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      location_bar_model_android.GetPageClassification(false));
}

TEST(LocationBarModelAndroidTest, ShouldTrimDisplayUrlAfterHostName_Enabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      chrome::android::kAndroidBottomBar,
      {{chrome::android::kAndroidBottomBarShowDomainOnlyParam.name, "true"}});

  TestLocationBarModelAndroid location_bar_model_android;
  location_bar_model_android.SetURL(GURL("https://www.example.com/path?q=1"));
  EXPECT_TRUE(location_bar_model_android.ShouldTrimDisplayUrlAfterHostName());
}

TEST(LocationBarModelAndroidTest,
     ShouldTrimDisplayUrlAfterHostName_ToolbarUiRefactorDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      chrome::android::kAndroidBottomBar,
      {{chrome::android::kAndroidBottomBarShowDomainOnlyParam.name, "true"}});

  TestLocationBarModelAndroid location_bar_model_android;
  location_bar_model_android.SetIsToolbarUiRefactorEnabled(false);
  location_bar_model_android.SetURL(GURL("https://www.example.com/path?q=1"));
  EXPECT_FALSE(location_bar_model_android.ShouldTrimDisplayUrlAfterHostName());
}

TEST(LocationBarModelAndroidTest,
     ShouldTrimDisplayUrlAfterHostName_NonHttpSchemeNotTrimmed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      chrome::android::kAndroidBottomBar,
      {{chrome::android::kAndroidBottomBarShowDomainOnlyParam.name, "true"}});

  TestLocationBarModelAndroid location_bar_model_android;
  location_bar_model_android.SetURL(GURL("chrome-native://bookmarks/folder/0"));
  EXPECT_FALSE(location_bar_model_android.ShouldTrimDisplayUrlAfterHostName());
}

TEST(LocationBarModelAndroidTest,
     ShouldTrimDisplayUrlAfterHostName_DefaultDisabled) {
  base::test::ScopedFeatureList feature_list(
      chrome::android::kAndroidBottomBar);

  TestLocationBarModelAndroid location_bar_model_android;
  location_bar_model_android.SetURL(GURL("https://www.example.com/path?q=1"));
  EXPECT_FALSE(location_bar_model_android.ShouldTrimDisplayUrlAfterHostName());
}

TEST(LocationBarModelAndroidTest,
     ShouldTrimDisplayUrlAfterHostName_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(chrome::android::kAndroidBottomBar);

  TestLocationBarModelAndroid location_bar_model_android;
  location_bar_model_android.SetURL(GURL("https://www.example.com/path?q=1"));
  EXPECT_FALSE(location_bar_model_android.ShouldTrimDisplayUrlAfterHostName());
}

TEST_F(LocationBarModelAndroidWebUiNtpTest, IsNewTabPage_WebUiNtp) {
  EnableDesktopAndroidWebUiNtp();
  TestLocationBarModelAndroid location_bar_model_android;

  location_bar_model_android.SetURL(GURL(chrome::kChromeUINewTabPageURL));
  EXPECT_TRUE(location_bar_model_android.IsNewTabPage());
  EXPECT_EQ(
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      location_bar_model_android.GetPageClassification(false));

  location_bar_model_android.SetURL(
      GURL(std::string(chrome::kChromeUINewTabPageURL) + "foopath?foo=bar"));
  EXPECT_TRUE(location_bar_model_android.IsNewTabPage());
}

TEST_F(LocationBarModelAndroidWebUiNtpTest, IsNewTabPage_ThirdPartyWebUiNtp) {
  EnableDesktopAndroidWebUiNtp();
  TestLocationBarModelAndroid location_bar_model_android;

  location_bar_model_android.SetURL(
      GURL(chrome::kChromeUINewTabPageThirdPartyURL));
  EXPECT_TRUE(location_bar_model_android.IsNewTabPage());
  EXPECT_EQ(
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      location_bar_model_android.GetPageClassification(false));

  location_bar_model_android.SetURL(
      GURL(std::string(chrome::kChromeUINewTabPageThirdPartyURL) +
           "foopath?foo=bar"));
  EXPECT_TRUE(location_bar_model_android.IsNewTabPage());
}

TEST_F(LocationBarModelAndroidWebUiNtpTest, IsNewTabPage_OtherChromeUrls) {
  EnableDesktopAndroidWebUiNtp();
  TestLocationBarModelAndroid location_bar_model_android;

  location_bar_model_android.SetURL(
      GURL("chrome://new-tab-page-third-partyx/"));
  EXPECT_FALSE(location_bar_model_android.IsNewTabPage());

  location_bar_model_android.SetURL(GURL("chrome://new-tab-pagex/"));
  EXPECT_FALSE(location_bar_model_android.IsNewTabPage());

  location_bar_model_android.SetURL(GURL("chrome://settings/"));
  EXPECT_FALSE(location_bar_model_android.IsNewTabPage());

  location_bar_model_android.SetURL(GURL("https://new-tab-page-third-party/"));
  EXPECT_FALSE(location_bar_model_android.IsNewTabPage());
}

TEST_F(LocationBarModelAndroidWebUiNtpTest, IsNewTabPage_WebUiNtpDisabled) {
  DisableDesktopAndroidWebUiNtp();
  TestLocationBarModelAndroid location_bar_model_android;

  location_bar_model_android.SetURL(GURL(chrome::kChromeUINewTabPageURL));
  EXPECT_FALSE(location_bar_model_android.IsNewTabPage());

  location_bar_model_android.SetURL(
      GURL(chrome::kChromeUINewTabPageThirdPartyURL));
  EXPECT_FALSE(location_bar_model_android.IsNewTabPage());

  location_bar_model_android.SetURL(GURL(chrome::kChromeUINativeNewTabURL));
  EXPECT_TRUE(location_bar_model_android.IsNewTabPage());
}

TEST_F(LocationBarModelAndroidWebUiNtpTest, IsNewTabPage_NotDesktop) {
  base::android::device_info::set_is_desktop_for_testing(false);
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kUseWebUiNtpAndroid);
  TestLocationBarModelAndroid location_bar_model_android;

  location_bar_model_android.SetURL(GURL(chrome::kChromeUINewTabPageURL));
  EXPECT_FALSE(location_bar_model_android.IsNewTabPage());

  location_bar_model_android.SetURL(
      GURL(chrome::kChromeUINewTabPageThirdPartyURL));
  EXPECT_FALSE(location_bar_model_android.IsNewTabPage());

  location_bar_model_android.SetURL(GURL(chrome::kChromeUINativeNewTabURL));
  EXPECT_TRUE(location_bar_model_android.IsNewTabPage());
}
