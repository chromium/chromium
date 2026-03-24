// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_aim_handler.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/webui/omnibox_popup/mojom/omnibox_popup_aim.mojom.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller_test_support.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/geometry/point.h"

namespace {

class MockOmniboxPopupAimPage : public omnibox_popup_aim::mojom::Page {
 public:
  MockOmniboxPopupAimPage() = default;
  ~MockOmniboxPopupAimPage() override = default;

  mojo::PendingRemote<omnibox_popup_aim::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnPopupShown,
              (searchbox::mojom::SearchContextPtr context),
              (override));
  MOCK_METHOD(void,
              AddContext,
              (searchbox::mojom::SearchContextPtr context),
              (override));
  MOCK_METHOD(void, FocusInput, (), (override));
  MOCK_METHOD(void, ClearPopup, (ClearPopupCallback callback), (override));
  MOCK_METHOD(void,
              SetPreserveContextOnClose,
              (bool preserve_context_on_close),
              (override));

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<omnibox_popup_aim::mojom::Page> receiver_{this};
};

class TestOmniboxPopupAimHandler : public OmniboxPopupAimHandler {
 public:
  using OmniboxPopupAimHandler::OmniboxPopupAimHandler;

 protected:
  OmniboxAimPopupWebUIContent* GetAimPopupContent() override { return nullptr; }
};

}  // namespace

class OmniboxPopupAimHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  OmniboxPopupAimHandlerTest() = default;
  ~OmniboxPopupAimHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_ui_.set_web_contents(web_contents());
    omnibox_popup_ui_ = std::make_unique<OmniboxPopupUI>(&web_ui_);
    handler_ = std::make_unique<TestOmniboxPopupAimHandler>(
        mojo::PendingReceiver<omnibox_popup_aim::mojom::PageHandler>(),
        page_.BindAndGetRemote(), web_contents());
    embedder_ = std::make_unique<TestEmbedder>();
    handler_->set_embedder(embedder_->GetWeakPtr());
  }

  void TearDown() override {
    handler_.reset();
    omnibox_popup_ui_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  content::TestWebUI web_ui_;
  std::unique_ptr<OmniboxPopupUI> omnibox_popup_ui_;
  testing::NiceMock<MockOmniboxPopupAimPage> page_;
  std::unique_ptr<TestEmbedder> embedder_;
  std::unique_ptr<OmniboxPopupAimHandler> handler_;
};

TEST_F(OmniboxPopupAimHandlerTest, ShowContextMenu) {
  gfx::Point point(10, 20);
  handler_->ShowContextMenu(point);
  EXPECT_TRUE(embedder_->context_menu_shown());
  EXPECT_EQ(point, *embedder_->last_context_menu_point());
}

TEST_F(OmniboxPopupAimHandlerTest, RequestClose) {
  EXPECT_FALSE(embedder_->ui_closed());
  handler_->RequestClose();
  EXPECT_TRUE(embedder_->ui_closed());
}

TEST_F(OmniboxPopupAimHandlerTest, OnPopupShown) {
  auto context = std::make_unique<SearchboxContextData::Context>();
  context->text = "test query";
  context->mode = omnibox::TOOL_MODE_UNSPECIFIED;

  EXPECT_CALL(page_, OnPopupShown(testing::_))
      .WillOnce([&](searchbox::mojom::SearchContextPtr context_ptr) {
        EXPECT_EQ("test query", context_ptr->input);
        EXPECT_EQ(omnibox::TOOL_MODE_UNSPECIFIED, context_ptr->tool_mode);
      });

  handler_->OnPopupShown(std::move(context));
}

TEST_F(OmniboxPopupAimHandlerTest, AddContext) {
  auto context = std::make_unique<SearchboxContextData::Context>();
  context->text = "added context";

  EXPECT_CALL(page_, AddContext(testing::_))
      .WillOnce([&](searchbox::mojom::SearchContextPtr context_ptr) {
        EXPECT_EQ("added context", context_ptr->input);
      });

  handler_->AddContext(std::move(context));
}

TEST_F(OmniboxPopupAimHandlerTest, ClearPopup) {
  EXPECT_CALL(page_, ClearPopup(testing::_))
      .WillOnce([](omnibox_popup_aim::mojom::Page::ClearPopupCallback callback) {
        std::move(callback).Run("final input");
      });

  base::test::TestFuture<const std::string&> future;
  handler_->ClearPopup(future.GetCallback());
  EXPECT_EQ("final input", future.Get());
}
