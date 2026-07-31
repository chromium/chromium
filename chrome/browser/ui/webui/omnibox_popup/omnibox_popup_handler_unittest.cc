// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_handler.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/test_omnibox_view.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller_test_support.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/geometry/point.h"

namespace {

class OmniboxPopupHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  OmniboxPopupHandlerTest() = default;
  ~OmniboxPopupHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_ui_.set_web_contents(web_contents());
    omnibox_popup_ui_ = std::make_unique<OmniboxPopupUI>(&web_ui_);
    handler_ = std::make_unique<OmniboxPopupHandler>(
        mojo::PendingReceiver<omnibox_popup::mojom::PageHandler>(),
        page_.BindAndGetRemote(), web_contents(),
        /*controller=*/nullptr);
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
  testing::NiceMock<MockOmniboxPopupPage> page_;
  std::unique_ptr<TestEmbedder> embedder_;
  std::unique_ptr<OmniboxPopupHandler> handler_;
};

TEST_F(OmniboxPopupHandlerTest, OnShow) {
  EXPECT_CALL(page_, OnShow());
  handler_->OnShow();
  page_.FlushForTesting();
}

TEST_F(OmniboxPopupHandlerTest, SetFocus) {
  EXPECT_CALL(page_, SetFocus(true));
  handler_->SetFocus(true);
  page_.FlushForTesting();
}

TEST_F(OmniboxPopupHandlerTest, ShowContextMenu) {
  handler_->ShowContextMenu(gfx::Point());
  EXPECT_TRUE(embedder_->context_menu_shown());
}

TEST_F(OmniboxPopupHandlerTest, SetInputState) {
  std::string test_text = "test input";
  gfx::Range test_selection(1, 5);
  std::string full_url = "test.com";
  std::string permanent_display_text = "permanent.com";
  bool show_full_url = true;
  bool query_zps = true;
  EXPECT_CALL(page_, SetInputState(testing::_))
      .WillOnce([&](omnibox_popup::mojom::OmniboxInputStatePtr state) {
        EXPECT_EQ(state->text, test_text);
        EXPECT_EQ(state->selection, test_selection);
        EXPECT_TRUE(state->user_input_in_progress);
        EXPECT_EQ(state->full_url, full_url);
        EXPECT_TRUE(state->is_focused);
        EXPECT_EQ(state->permanent_display_text, permanent_display_text);
        EXPECT_TRUE(state->show_full_url);
        EXPECT_TRUE(state->query_zps);
      });
  handler_->SetInputState(test_text, test_selection,
                          /*user_input_in_progress=*/true, full_url,
                          /*is_focused=*/true, permanent_display_text,
                          show_full_url, query_zps);
  page_.FlushForTesting();
}

TEST_F(OmniboxPopupHandlerTest, OnSelectionChanged) {
  gfx::Range test_selection(1, 5);
  handler_->OnSelectionChanged(test_selection, 0, false);
  EXPECT_EQ(handler_->latest_selection(), test_selection);
}

TEST_F(OmniboxPopupHandlerTest, OnSelectionChangedSequenceGuard) {
  // Fresh handler has sequence number 0. A call with sequence 0 is accepted.
  gfx::Range selection1(1, 5);
  handler_->OnSelectionChanged(selection1, 0, false);
  EXPECT_EQ(handler_->latest_selection(), selection1);

  // `SetInputState increments the sequence number to 1.
  handler_->SetInputState("test", gfx::Range(0, 0),
                          /*user_input_in_progress=*/false, /*full_url=*/"",
                          /*is_focused=*/true, /*permanent_display_text=*/"",
                          /*show_full_url=*/false, /*query_zps=*/false);

  // A call with stale sequence number 0 should be discarded.
  gfx::Range selection2(2, 6);
  handler_->OnSelectionChanged(selection2, 0, false);
  EXPECT_EQ(handler_->latest_selection(), gfx::Range(0, 0));

  // A call with active sequence number 1 should be accepted.
  handler_->OnSelectionChanged(selection2, 1, false);
  EXPECT_EQ(handler_->latest_selection(), selection2);
}

TEST_F(OmniboxPopupHandlerTest, OnInputClearedSynchronicity) {
  auto omnibox_controller = std::make_unique<OmniboxController>(
      std::make_unique<TestOmniboxClient>());
  auto test_omnibox_view =
      std::make_unique<TestOmniboxView>(omnibox_controller.get());
  omnibox_controller->edit_model()->set_view(test_omnibox_view.get());

  testing::NiceMock<MockOmniboxPopupPage> local_page;

  // Re-initialize handler_ with the non-null controller
  handler_ = std::make_unique<OmniboxPopupHandler>(
      mojo::PendingReceiver<omnibox_popup::mojom::PageHandler>(),
      local_page.BindAndGetRemote(), web_contents(), omnibox_controller.get());

  // Set some user text to ensure it's not empty initially.
  omnibox_controller->edit_model()->SetUserText(u"some text");
  test_omnibox_view->SetWindowTextAndCaretPos(u"some text", 0, false, false);
  EXPECT_EQ(omnibox_controller->edit_model()->user_text(), u"some text");
  EXPECT_EQ(test_omnibox_view->GetText(), u"some text");

  // Action: Invoke handler_->OnInputCleared(0);
  handler_->OnInputCleared(0);

  // Expected Assertions:
  // Assert that edit_model()->user_text() is empty (u"").
  // Assert that edit_model()->view()->GetText() is empty (u"").
  EXPECT_EQ(omnibox_controller->edit_model()->user_text(), u"");
  EXPECT_EQ(test_omnibox_view->GetText(), u"");

  // Reset the handler to avoid dangling raw_ptr to the local
  // omnibox_controller.
  handler_.reset();
}

TEST_F(OmniboxPopupHandlerTest, OnInputClearedSequenceGuard) {
  auto omnibox_controller = std::make_unique<OmniboxController>(
      std::make_unique<TestOmniboxClient>());
  auto test_omnibox_view =
      std::make_unique<TestOmniboxView>(omnibox_controller.get());
  omnibox_controller->edit_model()->set_view(test_omnibox_view.get());
  testing::NiceMock<MockOmniboxPopupPage> local_page;
  handler_ = std::make_unique<OmniboxPopupHandler>(
      mojo::PendingReceiver<omnibox_popup::mojom::PageHandler>(),
      local_page.BindAndGetRemote(), web_contents(), omnibox_controller.get());

  handler_->SetInputState("test", gfx::Range(0, 0),
                          /*user_input_in_progress=*/false, /*full_url=*/"",
                          /*is_focused=*/true, /*permanent_display_text=*/"",
                          /*show_full_url=*/false, /*query_zps=*/false);
  omnibox_controller->edit_model()->SetUserText(u"some text");
  test_omnibox_view->SetWindowTextAndCaretPos(u"some text", 0, false, false);

  handler_->OnInputCleared(/*sequence_number=*/0);
  EXPECT_EQ(omnibox_controller->edit_model()->user_text(), u"some text");
  EXPECT_EQ(test_omnibox_view->GetText(), u"some text");

  handler_->OnInputCleared(/*sequence_number=*/1);
  EXPECT_EQ(omnibox_controller->edit_model()->user_text(), u"");
  EXPECT_EQ(test_omnibox_view->GetText(), u"");

  handler_.reset();
}

TEST_F(OmniboxPopupHandlerTest, RevertSequenceGuard) {
  auto omnibox_controller = std::make_unique<OmniboxController>(
      std::make_unique<TestOmniboxClient>());
  auto test_omnibox_view =
      std::make_unique<TestOmniboxView>(omnibox_controller.get());
  omnibox_controller->edit_model()->set_view(test_omnibox_view.get());
  testing::NiceMock<MockOmniboxPopupPage> local_page;
  handler_ = std::make_unique<OmniboxPopupHandler>(
      mojo::PendingReceiver<omnibox_popup::mojom::PageHandler>(),
      local_page.BindAndGetRemote(), web_contents(), omnibox_controller.get());

  handler_->SetInputState("test", gfx::Range(0, 0),
                          /*user_input_in_progress=*/false, /*full_url=*/"",
                          /*is_focused=*/true, /*permanent_display_text=*/"",
                          /*show_full_url=*/false, /*query_zps=*/false);
  omnibox_controller->edit_model()->SetUserText(u"draft text");

  handler_->Revert(/*sequence_number=*/0);
  EXPECT_EQ(omnibox_controller->edit_model()->user_text(), u"draft text");

  handler_->Revert(/*sequence_number=*/1);
  EXPECT_FALSE(omnibox_controller->edit_model()->user_input_in_progress());

  handler_.reset();
}

}  // namespace
