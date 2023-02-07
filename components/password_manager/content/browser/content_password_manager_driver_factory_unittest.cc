// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory_test_api.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace password_manager {

// Fixture for testing that Password Manager is enabled in fenced frames unless
// EnablePasswordManagerWithinFencedFrame is enabled. The bool parameter
// enables/disables that feature.
class ContentPasswordManagerDriverFactoryFencedFramesTest
    : public content::RenderViewHostTestHarness,
      public ::testing::WithParamInterface<bool> {
 public:
  ContentPasswordManagerDriverFactoryFencedFramesTest() {
    std::vector<base::test::FeatureRefAndParams> enabled;
    std::vector<base::test::FeatureRef> disabled;
    enabled.push_back(
        {blink::features::kFencedFrames, {{"implementation_type", "mparch"}}});
    if (password_manager_enabled_in_fencedframe()) {
      enabled.push_back({blink::features::kFencedFramesAPIChanges, {}});
      enabled.push_back(
          {features::kEnablePasswordManagerWithinFencedFrame, {}});
    } else {
      disabled.push_back(features::kEnablePasswordManagerWithinFencedFrame);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

  ~ContentPasswordManagerDriverFactoryFencedFramesTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    factory_ = ContentPasswordManagerDriverFactoryTestApi::Create(
        web_contents(), &password_manager_client_, &autofill_client_);
  }

  void NavigateAndCommitInFrame(const std::string& url,
                                content::RenderFrameHost* rfh) {
    auto navigation =
        content::NavigationSimulator::CreateRendererInitiated(GURL(url), rfh);
    // These tests simulate loading events manually.
    navigation->SetKeepLoading(true);
    navigation->Start();
    navigation->Commit();
  }

  bool password_manager_enabled_in_fencedframe() const { return GetParam(); }

  ContentPasswordManagerDriverFactory& factory() { return *factory_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  autofill::TestAutofillClient autofill_client_;
  StubPasswordManagerClient password_manager_client_;
  std::unique_ptr<ContentPasswordManagerDriverFactory> factory_;
};

TEST_P(ContentPasswordManagerDriverFactoryFencedFramesTest,
       DisablePasswordManagerWithinFencedFrame) {
  NavigateAndCommitInFrame("http://test.org", main_rfh());
  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  content::RenderFrameHost* fenced_frame_subframe =
      content::RenderFrameHostTester::For(fenced_frame_root)
          ->AppendChild("iframe");
  EXPECT_NE(nullptr, factory().GetDriverForFrame(main_rfh()));
  if (password_manager_enabled_in_fencedframe()) {
    EXPECT_NE(nullptr, factory().GetDriverForFrame(fenced_frame_root));
    EXPECT_NE(nullptr, factory().GetDriverForFrame(fenced_frame_subframe));
  } else {
    EXPECT_EQ(nullptr, factory().GetDriverForFrame(fenced_frame_root));
    EXPECT_EQ(nullptr, factory().GetDriverForFrame(fenced_frame_subframe));
  }
}

INSTANTIATE_TEST_SUITE_P(ContentPasswordManagerDriverFactoryTest,
                         ContentPasswordManagerDriverFactoryFencedFramesTest,
                         testing::Bool());

}  // namespace password_manager
