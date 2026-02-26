// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_ui.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"
#include "ui/color/color_provider.h"

namespace {

// Test fixture for WebUIToolbarUIConfig.
class WebUIToolbarUIConfigTest : public testing::Test {
 public:
  WebUIToolbarUIConfigTest() = default;
  ~WebUIToolbarUIConfigTest() override = default;

  // Not movable or copyable.
  WebUIToolbarUIConfigTest(const WebUIToolbarUIConfigTest&) = delete;
  WebUIToolbarUIConfigTest& operator=(const WebUIToolbarUIConfigTest&) = delete;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

// Tests that the IsWebUIEnabled method returns the correct value based on the
// feature flag.
TEST_F(WebUIToolbarUIConfigTest, IsWebUIEnabled) {
  TestingProfile profile;
  WebUIToolbarConfig config;

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUIReloadButton},
        {features::kWebUISplitTabsButton});
    EXPECT_TRUE(config.IsWebUIEnabled(&profile));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kInitialWebUI, features::kWebUISplitTabsButton},
        {features::kWebUIReloadButton});
    EXPECT_TRUE(config.IsWebUIEnabled(&profile));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kInitialWebUI},
        {features::kWebUIReloadButton, features::kWebUISplitTabsButton});
    EXPECT_FALSE(config.IsWebUIEnabled(&profile));
  }
}

}  // namespace
