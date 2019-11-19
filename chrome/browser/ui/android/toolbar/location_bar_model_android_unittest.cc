// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/toolbar/location_bar_model_android.h"

#include "chrome/common/webui_url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestLocationBarModelAndroid : public LocationBarModelAndroid {
 public:
  TestLocationBarModelAndroid()
      : LocationBarModelAndroid(nullptr, base::android::JavaRef<jobject>()) {}
  ~TestLocationBarModelAndroid() override = default;

  // LocationBarModelDelegate:
  bool GetURL(GURL* url) const override {
    *url = url_;
    return true;
  }

  void SetURL(const GURL& url) { url_ = url; }

 private:
  GURL url_;
};

}  // namespace

TEST(LocationBarModelAndroidTest, ClassifyAndroidNativeNewTabPage) {
  TestLocationBarModelAndroid location_bar_model_android;
  location_bar_model_android.SetURL(GURL(chrome::kChromeUINativeNewTabURL));
  EXPECT_EQ(
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      location_bar_model_android.GetPageClassification(
          nullptr, base::android::JavaParamRef<jobject>(nullptr), false));

  std::string ntp_with_path_and_query =
      std::string(chrome::kChromeUINativeNewTabURL) + "foopath?foo=bar";
  location_bar_model_android.SetURL(GURL(ntp_with_path_and_query));
  EXPECT_EQ(
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      location_bar_model_android.GetPageClassification(
          nullptr, base::android::JavaParamRef<jobject>(nullptr), false));
}
