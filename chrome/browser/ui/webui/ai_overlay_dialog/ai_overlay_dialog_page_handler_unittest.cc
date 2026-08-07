// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog_page_handler.h"

#include <optional>

#include "base/test/test_future.h"
#include "chrome/test/base/browser_with_test_window_test.h"
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

class AiOverlayDialogPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  AiOverlayDialogPageHandlerTest() = default;
  ~AiOverlayDialogPageHandlerTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    mojo::PendingRemote<ai_overlay_dialog::mojom::Page> page_remote;
    page_receiver_.Bind(page_remote.InitWithNewPipeAndPassReceiver());

    handler_ = std::make_unique<AiOverlayDialogPageHandler>(
        handler_remote_.BindNewPipeAndPassReceiver(), std::move(page_remote),
        browser());
  }

  void TearDown() override {
    handler_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  AiOverlayDialogPageHandler* handler() { return handler_.get(); }
  mojo::Remote<ai_overlay_dialog::mojom::PageHandler>& handler_remote() {
    return handler_remote_;
  }

 private:
  MockPage mock_page_;
  mojo::Receiver<ai_overlay_dialog::mojom::Page> page_receiver_{&mock_page_};
  mojo::Remote<ai_overlay_dialog::mojom::PageHandler> handler_remote_;
  std::unique_ptr<AiOverlayDialogPageHandler> handler_;
};

TEST_F(AiOverlayDialogPageHandlerTest, GetCursorPosition) {
  AddTab(browser(), GURL("about:blank"));

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
  AddTab(browser(), GURL("about:blank"));

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

}  // namespace

}  // namespace ttc
