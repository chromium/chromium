// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_ui_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestWebUIContentsWrapper : public WebUIContentsWrapper {
 public:
  explicit TestWebUIContentsWrapper(Profile* profile)
      : WebUIContentsWrapper(GURL(""), profile, 0, true, true, true, "Test") {}
  ~TestWebUIContentsWrapper() override = default;

  void ReloadWebContents() override {}

  base::WeakPtr<WebUIContentsWrapper> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestWebUIContentsWrapper> weak_ptr_factory_{this};
};

}  // namespace

class OmniboxEverywhereControllerTest : public ChromeViewsTestBase {
 public:
  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  base::test::ScopedFeatureList feature_list_{omnibox::kOmniboxEverywhere};
  TestingProfile profile_;
};

TEST_F(OmniboxEverywhereControllerTest, EnabledFeatureInstantiatesController) {
  TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
      /*profile_manager=*/false);

  GlobalFeatures* features = TestingBrowserProcess::GetGlobal()->GetFeatures();
  ASSERT_TRUE(features);
  EXPECT_TRUE(features->omnibox_everywhere_controller());
}

TEST_F(OmniboxEverywhereControllerTest, OnInvokeControlsWidget) {
  omnibox_everywhere::OmniboxEverywhereController controller(
      base::BindRepeating(
          [](Profile* profile) -> std::unique_ptr<WebUIContentsWrapper> {
            return std::make_unique<TestWebUIContentsWrapper>(profile);
          }));

  EXPECT_FALSE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      &profile_, GetContext());
  EXPECT_TRUE(controller.IsVisible());

  controller.OnInvoke(omnibox_everywhere::InvocationSource::kGlobalHotkey,
                      &profile_, GetContext());
  EXPECT_FALSE(controller.IsVisible());
}
