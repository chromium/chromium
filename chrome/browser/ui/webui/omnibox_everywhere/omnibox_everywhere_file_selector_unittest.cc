// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_file_selector.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/views/widget/widget.h"

namespace {

class OmniboxEverywhereFileSelectorTest : public ChromeViewsTestBase {
 public:
  OmniboxEverywhereFileSelectorTest() = default;
  ~OmniboxEverywhereFileSelectorTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    auto* factory = ui::FakeSelectFileDialog::RegisterFactory();
    factory->SetOpenCallback(base::BindRepeating(
        [](int* count) { ++(*count); }, &dialog_opened_count_));
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->SetBounds(gfx::Rect(0, 0, 600, 420));
    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    ui::SelectFileDialog::SetFactory(nullptr);
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<OmniboxEverywhereFileSelector> CreateFileSelector() {
    return std::make_unique<OmniboxEverywhereFileSelector>(
        widget_->GetNativeWindow(),
        base::BindRepeating([](int* count) { ++(*count); }, &opened_count_),
        base::BindRepeating([](int* count) { ++(*count); }, &closed_count_),
        base::BindRepeating([](int* count) { ++(*count); }, &canceled_count_));
  }

  views::Widget* widget() { return widget_.get(); }
  int dialog_opened_count() const { return dialog_opened_count_; }
  int opened_count() const { return opened_count_; }
  int closed_count() const { return closed_count_; }
  int canceled_count() const { return canceled_count_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  int dialog_opened_count_ = 0;
  int opened_count_ = 0;
  int closed_count_ = 0;
  int canceled_count_ = 0;
};

#if BUILDFLAG(IS_MAC)
TEST_F(OmniboxEverywhereFileSelectorTest,
       DefersDialogUntilWidgetBoundsChange) {
  auto selector = CreateFileSelector();
  selector->OpenFileUploadDialog(/*web_contents=*/nullptr, /*is_image=*/true,
                                 /*edit_model=*/nullptr, std::nullopt,
                                 /*was_ai_mode_open=*/false);
  EXPECT_EQ(1, opened_count());
  EXPECT_EQ(0, dialog_opened_count());

  // Changing the widget height (including collapsing to a multi-line query
  // Composebox height like 300 DIP) immediately presents the native file dialog
  // without waiting for the fallback timer.
  widget()->SetBounds(gfx::Rect(0, 0, 600, 300));
  EXPECT_EQ(1, dialog_opened_count());

  // Advancing past the fallback timer must not open a duplicate dialog.
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(1, dialog_opened_count());
}

TEST_F(OmniboxEverywhereFileSelectorTest,
       FallbackTimerShowsDialogWhenBoundsDoNotChange) {
  auto selector = CreateFileSelector();
  selector->OpenFileUploadDialog(/*web_contents=*/nullptr, /*is_image=*/true,
                                 /*edit_model=*/nullptr, std::nullopt,
                                 /*was_ai_mode_open=*/false);
  EXPECT_EQ(1, opened_count());
  EXPECT_EQ(0, dialog_opened_count());

  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(0, dialog_opened_count());

  task_environment()->FastForwardBy(base::Milliseconds(50));
  EXPECT_EQ(1, dialog_opened_count());
}

TEST_F(OmniboxEverywhereFileSelectorTest,
       CancelCancelsPendingShowAndNotifiesClosedAndCanceled) {
  auto selector = CreateFileSelector();
  selector->OpenFileUploadDialog(/*web_contents=*/nullptr, /*is_image=*/true,
                                 /*edit_model=*/nullptr, std::nullopt,
                                 /*was_ai_mode_open=*/false);
  EXPECT_EQ(1, opened_count());
  EXPECT_EQ(0, closed_count());
  EXPECT_EQ(0, canceled_count());
  EXPECT_EQ(0, dialog_opened_count());

  selector->Cancel();
  EXPECT_EQ(1, closed_count());
  EXPECT_EQ(1, canceled_count());

  // Neither bounds change nor timer expiration may open the canceled dialog.
  widget()->SetBounds(gfx::Rect(0, 0, 600, 140));
  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(0, dialog_opened_count());

  // Subsequent cancel is a no-op.
  selector->Cancel();
  EXPECT_EQ(1, closed_count());
  EXPECT_EQ(1, canceled_count());
}
#else
TEST_F(OmniboxEverywhereFileSelectorTest, OpensDialogImmediatelyOnNonMac) {
  auto selector = CreateFileSelector();
  selector->OpenFileUploadDialog(/*web_contents=*/nullptr, /*is_image=*/true,
                                 /*edit_model=*/nullptr, std::nullopt,
                                 /*was_ai_mode_open=*/false);
  EXPECT_EQ(1, opened_count());
  EXPECT_EQ(1, dialog_opened_count());
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(OmniboxEverywhereFileSelectorTest,
       CancelWhileDialogOpenNotifiesClosedAndCanceled) {
  auto selector = CreateFileSelector();
  selector->OpenFileUploadDialog(/*web_contents=*/nullptr, /*is_image=*/true,
                                 /*edit_model=*/nullptr, std::nullopt,
                                 /*was_ai_mode_open=*/false);
#if BUILDFLAG(IS_MAC)
  widget()->SetBounds(gfx::Rect(0, 0, 600, 140));
#endif
  EXPECT_EQ(1, dialog_opened_count());

  selector->Cancel();
  EXPECT_EQ(1, closed_count());
  EXPECT_EQ(1, canceled_count());

  // Subsequent cancel is a no-op.
  selector->Cancel();
  EXPECT_EQ(1, closed_count());
  EXPECT_EQ(1, canceled_count());
}

TEST_F(OmniboxEverywhereFileSelectorTest,
       FileSelectionCanceledInvokesClosedAndCanceledCallbacks) {
  auto selector = CreateFileSelector();
  selector->OpenFileUploadDialog(/*web_contents=*/nullptr, /*is_image=*/true,
                                 /*edit_model=*/nullptr, std::nullopt,
                                 /*was_ai_mode_open=*/false);
#if BUILDFLAG(IS_MAC)
  widget()->SetBounds(gfx::Rect(0, 0, 600, 140));
#endif
  EXPECT_EQ(1, dialog_opened_count());

  selector->FileSelectionCanceled();
  EXPECT_EQ(1, closed_count());
  EXPECT_EQ(1, canceled_count());
}

}  // namespace
