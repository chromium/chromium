// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ttc/ttc_fre_dialog.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/constrained_window/constrained_window_views_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/metrics.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace ttc {

namespace {

class TestConstrainedWindowViewsClient
    : public constrained_window::ConstrainedWindowViewsClient {
 public:
  web_modal::ModalDialogHost* GetModalDialogHost(
      gfx::NativeWindow parent) override {
    return nullptr;
  }

  gfx::NativeView GetDialogHostView(gfx::NativeWindow parent) override {
    return views::Widget::GetWidgetForNativeWindow(parent)->GetNativeView();
  }
};

class TtcFreDialogTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    constrained_window::SetConstrainedWindowViewsClient(
        std::make_unique<TestConstrainedWindowViewsClient>());
    parent_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    parent_widget_->Show();
  }

  void TearDown() override {
    parent_widget_.reset();
    constrained_window::SetConstrainedWindowViewsClient(nullptr);
    ChromeViewsTestBase::TearDown();
  }

  std::unique_ptr<TtcFreDialog> ShowDialog() {
    auto dialog =
        std::make_unique<TtcFreDialog>(parent_widget_->GetNativeWindow(),
                                       on_closed_.Get(), on_learn_more_.Get());
    dialog->Show();
    task_environment()->AdvanceClock(views::GetDoubleClickInterval());
    return dialog;
  }

  template <typename T>
  T* GetView(TtcFreDialog& dialog, ui::ElementIdentifier id) {
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingViewAs<T>(
        id, views::ElementTrackerViews::GetContextForWidget(
                dialog.widget_for_testing()));
  }

  void CloseParentWidget() {
    parent_widget_->CloseNow();
    parent_widget_.reset();
  }

  base::MockCallback<TtcFreDialog::ResultCallback> on_closed_;
  base::MockRepeatingClosure on_learn_more_;

 private:
  std::unique_ptr<views::Widget> parent_widget_;
};

TEST_F(TtcFreDialogTest, AllowButtonAccepts) {
  std::unique_ptr<TtcFreDialog> dialog = ShowDialog();

  auto* const allow_button =
      GetView<views::Button>(*dialog, TtcFreDialog::kAllowButtonElementId);
  ASSERT_TRUE(allow_button);
  EXPECT_CALL(on_closed_, Run(true)).WillOnce([&dialog](bool) {
    dialog.reset();
  });
  views::test::ButtonTestApi(allow_button).NotifyDefaultMouseClick();
  EXPECT_EQ(dialog, nullptr);
}

TEST_F(TtcFreDialogTest, CancelButtonDeclines) {
  std::unique_ptr<TtcFreDialog> dialog = ShowDialog();

  auto* const cancel_button =
      GetView<views::Button>(*dialog, TtcFreDialog::kCancelButtonElementId);
  ASSERT_TRUE(cancel_button);
  EXPECT_CALL(on_closed_, Run(false));
  views::test::ButtonTestApi(cancel_button).NotifyDefaultMouseClick();
  EXPECT_EQ(dialog->widget_for_testing(), nullptr);
}

TEST_F(TtcFreDialogTest, LearnMoreRunsCallback) {
  std::unique_ptr<TtcFreDialog> dialog = ShowDialog();

  auto* const link =
      GetView<views::Link>(*dialog, TtcFreDialog::kLearnMoreLinkElementId);
  ASSERT_TRUE(link);
  EXPECT_CALL(on_learn_more_, Run()).Times(2);
  EXPECT_CALL(on_closed_, Run).Times(0);
  const ui::KeyEvent space(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                           ui::EF_NONE);
  link->OnKeyPressed(space);
  link->OnKeyPressed(space);
  EXPECT_NE(dialog->widget_for_testing(), nullptr);
}

TEST_F(TtcFreDialogTest, EscDeclines) {
  std::unique_ptr<TtcFreDialog> dialog = ShowDialog();
  views::Widget* const widget = dialog->widget_for_testing();

  EXPECT_CALL(on_closed_, Run(false));
  widget->GetFocusManager()->ProcessAccelerator(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  EXPECT_EQ(dialog->widget_for_testing(), nullptr);
}

TEST_F(TtcFreDialogTest, ParentWidgetClosingDeclines) {
  std::unique_ptr<TtcFreDialog> dialog = ShowDialog();
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      dialog->widget_for_testing());

  EXPECT_CALL(on_closed_, Run(false));
  CloseParentWidget();
  destroyed_waiter.Wait();
  EXPECT_EQ(dialog->widget_for_testing(), nullptr);
}

TEST_F(TtcFreDialogTest, DestroyingClosesWithoutCallback) {
  std::unique_ptr<TtcFreDialog> dialog = ShowDialog();
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      dialog->widget_for_testing());

  EXPECT_CALL(on_closed_, Run).Times(0);
  dialog.reset();
  destroyed_waiter.Wait();
}

}  // namespace

}  // namespace ttc
