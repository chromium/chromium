// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_searchbox_controller.h"

#include "chrome/browser/ui/webui/searchbox/lens_searchbox_handler.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

using ::testing::_;

namespace lens {

// Minimalist wrapper to override parent-dependent methods and isolate
// the specific lifecycle logic under investigation.
class TestLensSearchboxController : public LensSearchboxController {
 public:
  TestLensSearchboxController() : LensSearchboxController(nullptr) {}
  ~TestLensSearchboxController() override = default;

  metrics::OmniboxEventProto::PageClassification GetPageClassification()
      const override {
    // Bypasses navigating the uninitialized parent pointers to calculate
    // classification.
    return metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX;
  }
};

class LensSearchboxControllerTest : public testing::Test {
 public:
  LensSearchboxControllerTest() = default;
  ~LensSearchboxControllerTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), content::SiteInstance::Create(profile_.get()));
    controller_ = std::make_unique<TestLensSearchboxController>();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TestLensSearchboxController> controller_;
};

TEST_F(LensSearchboxControllerTest, FlushesPendingTextQueryOnHandlerSet) {
  controller_->OnSessionStart(/*suppress_contextualization=*/false);

  // Setup data that should be pushed once bound.
  const std::string expected_text = "test search query";
  controller_->SetSearchboxInputText(expected_text);

  // Setup mock page to listen for instructions.
  testing::NiceMock<MockSearchboxPage> mock_page;
  EXPECT_CALL(mock_page, SetInputText(expected_text)).Times(1);

  // Create a real handler but point it to our mock page via mojo.
  auto handler = std::make_unique<LensSearchboxHandler>(
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      mock_page.BindAndGetRemote(), profile_.get(), web_contents_.get(),
      controller_.get());

  // Act: This injection should now properly route to OnPageBound and fire.
  controller_->SetSidePanelSearchboxHandler(std::move(handler));

  // Verify execution over async boundary.
  mock_page.FlushForTesting();
}

TEST_F(LensSearchboxControllerTest, FlushesPendingThumbnailOnHandlerSet) {
  controller_->OnSessionStart(/*suppress_contextualization=*/false);

  // Setup data that should be pushed once bound.
  const std::string expected_uri = "data:image/jpeg;base64,abc";
  controller_->SetSearchboxThumbnail(expected_uri);

  // Setup mock page to listen for instructions.
  testing::NiceMock<MockSearchboxPage> mock_page;
  EXPECT_CALL(mock_page, SetThumbnail(expected_uri, false)).Times(1);

  auto handler = std::make_unique<LensSearchboxHandler>(
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      mock_page.BindAndGetRemote(), profile_.get(), web_contents_.get(),
      controller_.get());

  // Act: Injection must trigger OnPageBound successfully.
  controller_->SetContextualSearchboxHandler(std::move(handler));

  mock_page.FlushForTesting();
}

}  // namespace lens
