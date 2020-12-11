// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/settings_overridden_dialog_view.h"

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/extensions/settings_overridden_dialog_controller.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/views/test/widget_test.h"

namespace {

struct DialogState {
  bool shown = false;
  base::Optional<SettingsOverriddenDialogController::DialogResult> result;
};

// A dialog controller that updates the provided DialogState when the dialog
// is interacted with.
class TestDialogController : public SettingsOverriddenDialogController {
 public:
  explicit TestDialogController(DialogState* state)
      : state_(state),
        show_params_{base::ASCIIToUTF16("Dialog Title"),
                     base::ASCIIToUTF16("Dialog Body")} {}
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

  DialogState* const state_;
  const ShowParams show_params_;
};

}  // namespace

class SettingsOverriddenDialogViewUnitTest : public ChromeViewsTestBase {
 public:
  SettingsOverriddenDialogViewUnitTest() = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());

    // Create a widget to host the anchor view.
    anchor_widget_ = CreateTestWidget();
    anchor_widget_->Show();
  }

  void TearDown() override {
    anchor_widget_ = nullptr;
    ChromeViewsTestBase::TearDown();
  }

  SettingsOverriddenDialogView* CreateAndShowDialog(
      std::unique_ptr<TestDialogController> controller) {
    auto* dialog = new SettingsOverriddenDialogView(std::move(controller));
    dialog->Show(GetNativeAnchorWindow());
    return dialog;
  }

  gfx::NativeWindow GetNativeAnchorWindow() {
    return anchor_widget_->GetNativeWindow();
  }

  void CloseAnchorWindow() {
    // Move out the anchor widget since we'll be closing it.
    auto anchor_widget = std::move(anchor_widget_);
    views::test::WidgetDestroyedWaiter destroyed_waiter(anchor_widget.get());
    anchor_widget->Close();
    destroyed_waiter.Wait();
  }

 private:
  std::unique_ptr<views::Widget> anchor_widget_;
};

TEST_F(SettingsOverriddenDialogViewUnitTest,
       DialogControllerIsNotifiedWhenShown) {
  DialogState state;
  auto controller = std::make_unique<TestDialogController>(&state);
  auto* dialog = new SettingsOverriddenDialogView(std::move(controller));

  EXPECT_FALSE(state.shown);
  dialog->Show(GetNativeAnchorWindow());
  EXPECT_TRUE(state.shown);

  dialog->GetWidget()->CloseNow();
}

TEST_F(SettingsOverriddenDialogViewUnitTest, DialogResult_ChangeSettingsBack) {
  DialogState state;
  auto controller = std::make_unique<TestDialogController>(&state);
  auto* dialog = CreateAndShowDialog(std::move(controller));

  dialog->AcceptDialog();
  ASSERT_TRUE(state.result);
  EXPECT_EQ(
      SettingsOverriddenDialogController::DialogResult::kChangeSettingsBack,
      *state.result);
}

TEST_F(SettingsOverriddenDialogViewUnitTest, DialogResult_KeepNewSettings) {
  DialogState state;
  auto controller = std::make_unique<TestDialogController>(&state);
  auto* dialog = CreateAndShowDialog(std::move(controller));

  dialog->CancelDialog();
  ASSERT_TRUE(state.result);
  EXPECT_EQ(SettingsOverriddenDialogController::DialogResult::kKeepNewSettings,
            *state.result);
}

TEST_F(SettingsOverriddenDialogViewUnitTest, DialogResult_DismissDialog) {
  DialogState state;
  auto controller = std::make_unique<TestDialogController>(&state);
  auto* dialog = CreateAndShowDialog(std::move(controller));

  views::test::WidgetDestroyedWaiter destroyed_waiter(dialog->GetWidget());
  dialog->GetWidget()->Close();
  destroyed_waiter.Wait();
  ASSERT_TRUE(state.result);
  EXPECT_EQ(SettingsOverriddenDialogController::DialogResult::kDialogDismissed,
            *state.result);
}

TEST_F(SettingsOverriddenDialogViewUnitTest, DialogResult_CloseParentWidget) {
  DialogState state;
  auto controller = std::make_unique<TestDialogController>(&state);
  CreateAndShowDialog(std::move(controller));

  CloseAnchorWindow();
  ASSERT_TRUE(state.result);
  EXPECT_EQ(SettingsOverriddenDialogController::DialogResult::
                kDialogClosedWithoutUserAction,
            *state.result);
}
