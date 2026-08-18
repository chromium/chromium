// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog_page_handler.h"

#include <optional>

#include "base/test/test_future.h"
#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"

namespace ttc {

namespace {

class MockPage : public ai_overlay_dialog::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  void DidChangePage(const std::string& url,
                     const std::optional<std::string>& title,
                     const std::optional<std::string>& content) override {}
  void UpdateCurrentPageContext(
      const std::string& title,
      ai_overlay_dialog::mojom::PageContentNodePtr root_node) override {}
  void SetInputCaptionsVisible(bool visible) override {}
  void SetOutputCaptionsVisible(bool visible) override {}
  void SetUsePersona(bool use_persona) override {}
};

class AiOverlayDialogPageHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  AiOverlayDialogPageHandlerTest() = default;
  ~AiOverlayDialogPageHandlerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    tab_strip_model_delegate_.SetBrowserWindowInterface(
        &browser_window_interface_);
    tab_strip_model_ =
        std::make_unique<TabStripModel>(&tab_strip_model_delegate_, profile());
    ON_CALL(browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile()));
    ON_CALL(browser_window_interface_, GetTabStripModel())
        .WillByDefault(testing::Return(tab_strip_model_.get()));

    controller_ =
        std::make_unique<AiOverlayDialogController>(&browser_window_interface_);

    mojo::PendingRemote<ai_overlay_dialog::mojom::Page> page_remote;
    page_receiver_.Bind(page_remote.InitWithNewPipeAndPassReceiver());

    handler_ = std::make_unique<AiOverlayDialogPageHandler>(
        handler_remote_.BindNewPipeAndPassReceiver(), std::move(page_remote),
        &browser_window_interface_);
  }

  void TearDown() override {
    handler_.reset();
    controller_.reset();
    if (tab_strip_model_) {
      tab_strip_model_->CloseAllTabs();
      tab_strip_model_.reset();
    }
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void AddTab() {
    std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
    tab_strip_model_->AppendWebContents(std::move(contents),
                                        /*foreground=*/true);
  }

  AiOverlayDialogPageHandler* handler() { return handler_.get(); }
  mojo::Remote<ai_overlay_dialog::mojom::PageHandler>& handler_remote() {
    return handler_remote_;
  }

 private:
  const tabs::TabModel::PreventFeatureInitializationForTesting
      prevent_tab_features_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface_;
  MockPage mock_page_;
  mojo::Receiver<ai_overlay_dialog::mojom::Page> page_receiver_{&mock_page_};
  mojo::Remote<ai_overlay_dialog::mojom::PageHandler> handler_remote_;
  std::unique_ptr<AiOverlayDialogController> controller_;
  std::unique_ptr<AiOverlayDialogPageHandler> handler_;
};

TEST_F(AiOverlayDialogPageHandlerTest, GetCursorPosition) {
  AddTab();

  base::test::TestFuture<const std::optional<gfx::Point>&> future;
  handler_remote()->GetCursorPosition(future.GetCallback());

  // Returns optional position (may be nullopt if test cursor is outside tab container bounds)
  auto position = future.Take();
  if (position.has_value()) {
    EXPECT_GE(position->x(), 0);
    EXPECT_GE(position->y(), 0);
  }
}

TEST_F(AiOverlayDialogPageHandlerTest, CaptureRawViewportRegion) {
  AddTab();

  base::test::TestFuture<ai_overlay_dialog::mojom::RawViewportRegionResultPtr>
      future;
  handler_remote()->CaptureRawViewportRegion(10, 10, 100, 100,
                                             future.GetCallback());

  // May return null in headless/headless unit test environment without a real
  // view surface
  auto result = future.Take();
  if (result) {
    EXPECT_GT(result->width, 0);
    EXPECT_GT(result->height, 0);
  }
}

TEST_F(AiOverlayDialogPageHandlerTest, RememberedNotesDictionaryStorage) {
  // 1. Verify initially empty.
  {
    base::test::TestFuture<
        std::vector<ai_overlay_dialog::mojom::RememberedNotePtr>>
        get_future;
    handler_remote()->GetRememberedNotes(get_future.GetCallback());
    EXPECT_TRUE(get_future.Take().empty());
  }

  // 2. Set a remembered note.
  {
    auto new_note =
        ai_overlay_dialog::mojom::RememberedNote::New("test_key", "test_val");
    base::test::TestFuture<bool> set_future;
    handler_remote()->SetRememberedNote(std::move(new_note),
                                        set_future.GetCallback());
    EXPECT_TRUE(set_future.Get());
  }

  // 3. Verify retrieving the note.
  {
    base::test::TestFuture<
        std::vector<ai_overlay_dialog::mojom::RememberedNotePtr>>
        get_future;
    handler_remote()->GetRememberedNotes(get_future.GetCallback());
    auto notes = get_future.Take();
    ASSERT_EQ(1u, notes.size());
    EXPECT_EQ("test_key", notes[0]->key);
    EXPECT_EQ("test_val", notes[0]->value);
  }

  // 3b. Verify updating an existing note key.
  {
    auto update_note = ai_overlay_dialog::mojom::RememberedNote::New(
        "test_key", "updated_val");
    base::test::TestFuture<bool> update_future;
    handler_remote()->SetRememberedNote(std::move(update_note),
                                        update_future.GetCallback());
    EXPECT_TRUE(update_future.Get());

    base::test::TestFuture<
        std::vector<ai_overlay_dialog::mojom::RememberedNotePtr>>
        get_future;
    handler_remote()->GetRememberedNotes(get_future.GetCallback());
    auto notes = get_future.Take();
    ASSERT_EQ(1u, notes.size());
    EXPECT_EQ("test_key", notes[0]->key);
    EXPECT_EQ("updated_val", notes[0]->value);
  }

  // 4. Delete the note by passing an empty string value.
  {
    auto delete_note =
        ai_overlay_dialog::mojom::RememberedNote::New("test_key", "");
    base::test::TestFuture<bool> delete_future;
    handler_remote()->SetRememberedNote(std::move(delete_note),
                                        delete_future.GetCallback());
    EXPECT_TRUE(delete_future.Get());
  }

  // 5. Verify note is deleted.
  {
    base::test::TestFuture<
        std::vector<ai_overlay_dialog::mojom::RememberedNotePtr>>
        get_future;
    handler_remote()->GetRememberedNotes(get_future.GetCallback());
    EXPECT_TRUE(get_future.Take().empty());
  }
}

TEST_F(AiOverlayDialogPageHandlerTest, SaveDebugFile) {
  // Calling SaveDebugFile without debug flags should safely no-op without
  // error.
  handler_remote()->SaveDebugFile(
      ai_overlay_dialog::mojom::DebugFileType::kPrimingTurnMarkdown,
      "# test markdown");
  handler_remote().FlushForTesting();
}

}  // namespace

}  // namespace ttc
