// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/extensions/chooser_dialog_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/device_chooser_content_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/permissions/fake_bluetooth_chooser_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/widget/widget.h"

using permissions::FakeBluetoothChooserController;

class ChooserDialogViewTest : public ChromeViewsTestBase {
 public:
  ChooserDialogViewTest() {}

  ChooserDialogViewTest(const ChooserDialogViewTest&) = delete;
  ChooserDialogViewTest& operator=(const ChooserDialogViewTest&) = delete;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    auto controller = std::make_unique<FakeBluetoothChooserController>();
    controller_ = controller.get();
    dialog_ = new ChooserDialogView(std::move(controller));
    // Must be called after a view has registered itself with the controller.
    controller_->SetBluetoothStatus(
        FakeBluetoothChooserController::BluetoothStatus::IDLE);

    gfx::NativeView parent = gfx::NativeView();
#if BUILDFLAG(IS_MAC)
    // We need a native view parent for the dialog to avoid a DCHECK
    // on Mac.
    parent_widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    parent = parent_widget_->GetNativeView();
#endif
    widget_ = views::DialogDelegate::CreateDialogWidget(dialog_, GetContext(),
                                                        parent);
    widget_->SetVisibilityChangedAnimationsEnabled(false);
    widget_->Show();
#if BUILDFLAG(IS_MAC)
    // Necessary for Mac. On other platforms this happens in the focus
    // manager, but it's disabled for Mac due to crbug.com/650859.
    parent_widget_->Activate();
    widget_->Activate();
#endif

    ASSERT_NE(nullptr, table_view());
    ASSERT_NE(nullptr, re_scan_button());
  }

  void TearDown() override {
    widget_->Close();
    parent_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  views::TableView* table_view() {
    return dialog_->device_chooser_content_view_for_test()
        ->table_view_for_testing();
  }

  views::LabelButton* re_scan_button() {
    return dialog_->device_chooser_content_view_for_test()
        ->ReScanButtonForTesting();
  }

  void AddDevice() {
    controller_->AddDevice(
        {"Device", FakeBluetoothChooserController::NOT_CONNECTED,
         FakeBluetoothChooserController::NOT_PAIRED,
         FakeBluetoothChooserController::kSignalStrengthLevel1});
  }

 protected:
  raw_ptr<ChooserDialogView, DanglingUntriaged> dialog_ = nullptr;
  raw_ptr<FakeBluetoothChooserController, DanglingUntriaged> controller_ =
      nullptr;

 private:
  std::unique_ptr<views::Widget> parent_widget_;
  raw_ptr<views::Widget, DanglingUntriaged> widget_ = nullptr;
};

TEST_F(ChooserDialogViewTest, ButtonState) {
  // Cancel button is always enabled.
  EXPECT_TRUE(dialog_->IsDialogButtonEnabled(ui::mojom::DialogButton::kCancel));

  // Selecting a device enables the OK button.
  EXPECT_FALSE(dialog_->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  AddDevice();
  EXPECT_FALSE(dialog_->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  table_view()->Select(0);
  EXPECT_TRUE(dialog_->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  // Changing state disables the OK button.
  controller_->SetBluetoothStatus(
      FakeBluetoothChooserController::BluetoothStatus::UNAVAILABLE);
  EXPECT_FALSE(dialog_->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  controller_->SetBluetoothStatus(
      FakeBluetoothChooserController::BluetoothStatus::SCANNING);
  EXPECT_FALSE(dialog_->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  table_view()->Select(0);
  EXPECT_TRUE(dialog_->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
  controller_->SetBluetoothStatus(
      FakeBluetoothChooserController::BluetoothStatus::IDLE);
  EXPECT_FALSE(dialog_->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
}

TEST_F(ChooserDialogViewTest, CancelButtonFocusedWhenReScanIsPressed) {
  EXPECT_CALL(*controller_, RefreshOptions())
      .WillOnce(testing::Invoke([=, this]() {
        controller_->SetBluetoothStatus(
            FakeBluetoothChooserController::BluetoothStatus::SCANNING);
      }));
  AddDevice();
  table_view()->RequestFocus();
  controller_->RemoveDevice(0);

  // Click the re-scan button.
  const gfx::Point point(10, 10);
  const ui::MouseEvent event(ui::EventType::kMousePressed, point, point,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  re_scan_button()->OnMousePressed(event);
  re_scan_button()->OnMouseReleased(event);

  EXPECT_FALSE(re_scan_button()->GetVisible());
  EXPECT_EQ(dialog_->GetCancelButton(),
            dialog_->GetFocusManager()->GetFocusedView());
}

TEST_F(ChooserDialogViewTest, Accept) {
  AddDevice();
  AddDevice();
  table_view()->Select(1);
  std::vector<size_t> expected = {1u};
  EXPECT_CALL(*controller_, Select(testing::Eq(expected))).Times(1);
  dialog_->Accept();
}

TEST_F(ChooserDialogViewTest, Cancel) {
  EXPECT_CALL(*controller_, Cancel()).Times(1);
  dialog_->Cancel();
}

TEST_F(ChooserDialogViewTest, Close) {
  // Called from Widget::Close() in TearDown().
  EXPECT_CALL(*controller_, Close()).Times(1);
}
