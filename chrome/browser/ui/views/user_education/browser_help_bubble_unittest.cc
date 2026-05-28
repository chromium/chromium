// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/user_education/views/help_bubble_delegate.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/webui/resources/js/tracked_element/tracked_element.mojom.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId);

class MockHelpBubbleClient : public help_bubble::mojom::HelpBubbleClient {
 public:
  MOCK_METHOD(void,
              ShowHelpBubble,
              (help_bubble::mojom::HelpBubbleParamsPtr data),
              (override));
  MOCK_METHOD(void,
              ToggleFocusForAccessibility,
              (const std::string& native_identifier),
              (override));
  MOCK_METHOD(void,
              HideHelpBubble,
              (const std::string& native_identifier),
              (override));
  MOCK_METHOD(void,
              ExternalHelpBubbleUpdated,
              (const std::string& native_identifier, bool shown),
              (override));
};

class TestHelpBubbleHandler : public user_education::HelpBubbleHandlerBase {
 public:
  TestHelpBubbleHandler(GetWebContentsCallback get_web_contents_callback,
                        ui::ElementContext context)
      : HelpBubbleHandlerBase(
            std::make_unique<ClientProvider>(),
            get_web_contents_callback,
            std::vector<ui::ElementIdentifier>{kTestElementId},
            context) {}

  void BindTrackedElementHandlerForTesting() {
    BindTrackedElementHandler(
        mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>());
  }

 private:
  class ClientProvider : public HelpBubbleHandlerBase::ClientProvider {
   public:
    help_bubble::mojom::HelpBubbleClient* GetClient() override {
      return &client_;
    }

   private:
    testing::NiceMock<MockHelpBubbleClient> client_;
  };
};

class MockHelpBubbleDelegate : public user_education::HelpBubbleDelegate {
 public:
  MOCK_METHOD(std::vector<ui::Accelerator>,
              GetPaneNavigationAccelerators,
              (ui::TrackedElement*),
              (const, override));
  MOCK_METHOD(int, GetTitleTextContext, (), (const, override));
  MOCK_METHOD(int, GetBodyTextContext, (), (const, override));
  MOCK_METHOD(ui::ColorId,
              GetHelpBubbleBackgroundColorId,
              (),
              (const, override));
  MOCK_METHOD(ui::ColorId,
              GetHelpBubbleForegroundColorId,
              (),
              (const, override));
  MOCK_METHOD(ui::ColorId,
              GetHelpBubbleDefaultButtonBackgroundColorId,
              (),
              (const, override));
  MOCK_METHOD(ui::ColorId,
              GetHelpBubbleDefaultButtonForegroundColorId,
              (),
              (const, override));
  MOCK_METHOD(ui::ColorId,
              GetHelpBubbleButtonBorderColorId,
              (),
              (const, override));
  MOCK_METHOD(ui::ColorId,
              GetHelpBubbleCloseButtonInkDropColorId,
              (),
              (const, override));
};

}  // namespace

class BrowserHelpBubbleUnitTest : public ChromeRenderViewHostTestHarness {
 public:
  BrowserHelpBubbleUnitTest() = default;
  ~BrowserHelpBubbleUnitTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    handler_ = std::make_unique<TestHelpBubbleHandler>(
        base::BindRepeating(
            [](content::WebContents* contents) { return contents; },
            web_contents_.get()),
        ui::ElementContext::CreateFakeContextForTesting(this));
    handler_->BindTrackedElementHandlerForTesting();
    anchor_ = std::make_unique<ui::TrackedElementWebUI>(
        handler_->GetTrackedElementHandlerForTesting(), kTestElementId,
        ui::ElementContext::CreateFakeContextForTesting(this));
  }

  void TearDown() override {
    anchor_.reset();
    handler_.reset();
    web_contents_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TestHelpBubbleHandler> handler_;
  std::unique_ptr<ui::TrackedElementWebUI> anchor_;
  testing::NiceMock<MockHelpBubbleDelegate> delegate_;
};

TEST_F(BrowserHelpBubbleUnitTest, ForceWebUIHelpBubbles_Logic) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kForcedId1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kForcedId2);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kUnforcedId);

  ForceWebUIHelpBubbles::CreateForWebContents(web_contents_.get());
  auto* forced = ForceWebUIHelpBubbles::FromWebContents(web_contents_.get());
  forced->SetForceWebUIForAnchors({kForcedId1, kForcedId2});

  EXPECT_TRUE(forced->ShouldForceWebUIForAnchor(kForcedId1));
  EXPECT_TRUE(forced->ShouldForceWebUIForAnchor(kForcedId2));
  EXPECT_FALSE(forced->ShouldForceWebUIForAnchor(kUnforcedId));
}

TEST_F(BrowserHelpBubbleUnitTest,
       CanBuildBubbleForTrackedElement_MatchingForcedAnchor) {
  FloatingWebUIHelpBubbleFactoryBrowser factory(&delegate_);
  ForceWebUIHelpBubbles::CreateForWebContents(web_contents_.get());
  auto* forced = ForceWebUIHelpBubbles::FromWebContents(web_contents_.get());
  forced->SetForceWebUIForAnchors({kTestElementId});
  EXPECT_FALSE(factory.CanBuildBubbleForTrackedElement(anchor_.get()));
}
