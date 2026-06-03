// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_overridden_dialog.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

using DialogResult = SettingsOverriddenDialogController::DialogResult;

namespace {

struct DialogState {
  bool shown = false;
  std::optional<DialogResult> result;
};

// A dialog controller that updates the provided DialogState when the dialog
// is interacted with.
class TestDialogController : public SettingsOverriddenDialogController {
 public:
  explicit TestDialogController(DialogState* state)
      : state_(state),
        show_params_(u"Dialog Title", u"Dialog Body", /*icon=*/nullptr) {}
  TestDialogController(const TestDialogController&) = delete;
  TestDialogController& operator=(const TestDialogController&) = delete;
  ~TestDialogController() override = default;

 private:
  bool ShouldShow() override { return true; }
  ShowParams GetShowParams() override { return show_params_; }
  void OnDialogShown() override {
    EXPECT_FALSE(state_->shown) << "OnDialogShown() called more than once!";
    state_->shown = true;
  }
  void HandleDialogResult(DialogResult result) override {
    state_->result = result;
  }

  const raw_ptr<DialogState> state_;
  const ShowParams show_params_;
};

}  // namespace

class SettingsOverriddenDialogUnitTest : public ChromeViewsTestBase {
  // TODO(crbug.com/424013924): Remove browser dependency and enable test for
  // Desktop Android
 public:
  SettingsOverriddenDialogUnitTest() = default;
  SettingsOverriddenDialogUnitTest(const SettingsOverriddenDialogUnitTest&) =
      delete;
  const SettingsOverriddenDialogUnitTest& operator=(
      const SettingsOverriddenDialogUnitTest&) = delete;
  ~SettingsOverriddenDialogUnitTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    constrained_window::SetConstrainedWindowViewsClient(
        CreateChromeConstrainedWindowViewsClient());

    parent_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    parent_widget_->Show();
  }

  void TearDown() override {
    parent_widget_.reset();
    constrained_window::SetConstrainedWindowViewsClient(nullptr);
    ChromeViewsTestBase::TearDown();
  }

  views::Widget* ShowDialog(DialogState* state) {
    auto controller = std::make_unique<TestDialogController>(state);
    EXPECT_FALSE(state->shown);

    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        kExtensionSettingsOverriddenDialogName);
    extensions::ShowSettingsOverriddenDialog(std::move(controller),
                                             parent_widget_->GetNativeWindow());
    views::Widget* dialog = waiter.WaitIfNeededAndGet();
    EXPECT_TRUE(state->shown);

    return dialog;
  }

 private:
  std::unique_ptr<views::Widget> parent_widget_;
};

TEST_F(SettingsOverriddenDialogUnitTest, DialogResult_ChangeSettingsBack) {
  DialogState state;
  views::Widget* dialog = ShowDialog(&state);

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->widget_delegate()->AsDialogDelegate()->AcceptDialog();
  dialog_waiter.Wait();

  ASSERT_TRUE(state.result);
  EXPECT_EQ(DialogResult::kChangeSettingsBack, state.result);
}

TEST_F(SettingsOverriddenDialogUnitTest, DialogResult_KeepNewSettings) {
  DialogState state;
  views::Widget* dialog = ShowDialog(&state);

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->widget_delegate()->AsDialogDelegate()->CancelDialog();
  dialog_waiter.Wait();

  ASSERT_TRUE(state.result);
  EXPECT_EQ(DialogResult::kKeepNewSettings, state.result);
}

TEST_F(SettingsOverriddenDialogUnitTest, DialogResult_DismissDialog) {
  DialogState state;
  views::Widget* dialog = ShowDialog(&state);

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->Close();
  dialog_waiter.Wait();

  ASSERT_TRUE(state.result);
  EXPECT_EQ(DialogResult::kDialogDismissed, state.result);
}

TEST_F(SettingsOverriddenDialogUnitTest, DialogResult_CloseParentWidget) {
  DialogState state;
  views::Widget* dialog = ShowDialog(&state);

  views::test::WidgetDestroyedWaiter dialog_waiter(dialog);
  dialog->CloseNow();
  dialog_waiter.Wait();

  ASSERT_TRUE(state.result);
  EXPECT_EQ(DialogResult::kDialogClosedWithoutUserAction, state.result);
}
