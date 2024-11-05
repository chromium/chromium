// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/autofill_form_features_injector.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/features.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

using autofill::AutofillFormFeaturesInjector;
using base::test::ScopedFeatureList;
using testing::UnorderedElementsAre;
using web::FakeWebFrame;

constexpr char kOrigin[] = "https://example.com";

// Test fixture for AutofillFormInjector.
class AutofillFormInjectorTest : public PlatformTest {
 public:
  AutofillFormInjectorTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    fake_web_frames_manager_ = frames_manager.get();
    fake_web_state_.SetWebFramesManager(web::ContentWorld::kPageContentWorld,
                                        std::move(frames_manager));
    std::unique_ptr<FakeWebFrame> main_frame =
        FakeWebFrame::CreateMainWebFrame(GURL(kOrigin));
    fake_web_frames_manager_->AddWebFrame(std::move(main_frame));
  }

 protected:
  void AddFrame(const std::string& frame_id) {
    std::unique_ptr<FakeWebFrame> web_frame =
        FakeWebFrame::Create(frame_id, false, GURL(kOrigin));
    fake_web_frames_manager_->AddWebFrame(std::move(web_frame));
  }

  web::FakeWebState fake_web_state_;
  raw_ptr<web::FakeWebFramesManager> fake_web_frames_manager_ = nullptr;
};

// Tests that the injector sets feature flags both in WebFrames existing when
// the injector is created and in new frames added afterwards.
TEST_F(AutofillFormInjectorTest, InjectFlagsWebFrames) {
  ScopedFeatureList features;
  features.InitWithFeatures(
      /* enabled_features= */ {kAutofillIsolatedWorldForJavascriptIos,
                               autofill::features::kAutofillAcrossIframesIos},
      /* disabled_features= */ {});

  AutofillFormFeaturesInjector injector(&fake_web_state_,
                                        web::ContentWorld::kPageContentWorld);
  AddFrame(web::kChildFakeFrameId);

  for (auto* web_frame : fake_web_frames_manager_->GetAllWebFrames()) {
    auto* fake_frame = static_cast<FakeWebFrame*>(web_frame);

    EXPECT_THAT(fake_frame->GetJavaScriptCallHistory(),
                UnorderedElementsAre(u"__gCrWeb.autofill_form_features."
                                     u"setAutofillIsolatedContentWorld(true);",
                                     u"__gCrWeb.autofill_form_features."
                                     u"setAutofillAcrossIframes(true);"));
  }
}

}  // namespace
