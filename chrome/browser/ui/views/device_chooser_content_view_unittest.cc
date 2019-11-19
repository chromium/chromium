// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/device_chooser_content_view.h"

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chooser_controller/fake_bluetooth_chooser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/controls/throbber.h"

namespace {

class MockTableViewObserver : public views::TableViewObserver {
 public:
  // views::TableViewObserver:
  MOCK_METHOD0(OnSelectionChanged, void());
};

}  // namespace

class DeviceChooserContentViewTest : public ChromeViewsTestBase {
 public:
  DeviceChooserContentViewTest() {}

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    table_observer_ = std::make_unique<MockTableViewObserver>();
    auto controller = std::make_unique<FakeBluetoothChooserController>();
    controller_ = controller.get();
    content_view_ = std::make_unique<DeviceChooserContentView>(
        table_observer_.get(), std::move(controller));

    // Also creates |bluetooth_status_container_|.
    extra_views_container_ = content_view().CreateExtraView();

    ASSERT_NE(nullptr, table_view());
    ASSERT_NE(nullptr, no_options_view());
    ASSERT_NE(nullptr, adapter_off_view());
    ASSERT_NE(nullptr, re_scan_button());
    ASSERT_NE(nullptr, throbber());
    ASSERT_NE(nullptr, scanning_label());

    controller_->SetBluetoothStatus(
        FakeBluetoothChooserController::BluetoothStatus::IDLE);
  }

  FakeBluetoothChooserController* controller() { return controller_; }
  MockTableViewObserver& table_observer() { return *table_observer_; }
  DeviceChooserContentView& content_view() { return *content_view_; }

  views::TableView* table_view() {
    return content_view().table_view_for_testing();
  }
  views::View* table_parent() { return content_view().table_parent_; }
  ui::TableModel* table_model() { return table_view()->model(); }
  views::View* no_options_view() { return content_view().no_options_view_; }
  views::View* adapter_off_view() { return content_view().adapter_off_view_; }
  views::LabelButton* re_scan_button() {
    return content_view().ReScanButtonForTesting();
  }
  views::Throbber* throbber() { return content_view().ThrobberForTesting(); }
  views::Label* scanning_label() {
    return content_view().ScanningLabelForTesting();
  }

  void AddUnpairedDevice() {
    controller()->AddDevice(
        {"Unpaired Device", FakeBluetoothChooserController::NOT_CONNECTED,
         FakeBluetoothChooserController::NOT_PAIRED,
         FakeBluetoothChooserController::kSignalStrengthLevel1});
  }

  void AddPairedDevice() {
    controller()->AddDevice(
        {"Paired Device", FakeBluetoothChooserController::CONNECTED,
         FakeBluetoothChooserController::PAIRED,
         FakeBluetoothChooserController::kSignalStrengthUnknown});
  }

  base::string16 GetUnpairedDeviceTextAtRow(size_t row_index) {
    return controller()->GetOption(row_index);
  }

  base::string16 GetPairedDeviceTextAtRow(size_t row_index) {
    return l10n_util::GetStringFUTF16(
        IDS_DEVICE_CHOOSER_DEVICE_NAME_AND_PAIRED_STATUS_TEXT,
        GetUnpairedDeviceTextAtRow(row_index));
  }

  bool IsDeviceSelected() { return !table_view()->selection_model().empty(); }

  void ExpectNoDevices() {
    EXPECT_TRUE(no_options_view()->GetVisible());
    EXPECT_EQ(0, table_view()->GetRowCount());
    // The table should be disabled since there are no (real) options.
    EXPECT_FALSE(table_parent()->GetVisible());
    EXPECT_FALSE(table_view()->GetEnabled());
    EXPECT_FALSE(IsDeviceSelected());
  }

 protected:
  std::unique_ptr<views::View> extra_views_container_;

 private:
  std::unique_ptr<MockTableViewObserver> table_observer_;
  FakeBluetoothChooserController* controller_ = nullptr;
  std::unique_ptr<DeviceChooserContentView> content_view_;

  DISALLOW_COPY_AND_ASSIGN(DeviceChooserContentViewTest);
};

TEST_F(DeviceChooserContentViewTest, InitialState) {
  EXPECT_CALL(table_observer(), OnSelectionChanged()).Times(0);

  ExpectNoDevices();
  EXPECT_FALSE(adapter_off_view()->GetVisible());
  EXPECT_FALSE(throbber()->GetVisible());
  EXPECT_FALSE(scanning_label()->GetVisible());
  EXPECT_TRUE(re_scan_button()->GetVisible());
  EXPECT_TRUE(re_scan_button()->GetEnabled());
}

TEST_F(DeviceChooserContentViewTest, AddOption) {
  EXPECT_CALL(table_observer(), OnSelectionChanged()).Times(0);
  AddPairedDevice();

  EXPECT_EQ(1, table_view()->GetRowCount());
  EXPECT_EQ(GetPairedDeviceTextAtRow(0), table_model()->GetText(0, 0));
  // The table should be enabled now that there's an option.
  EXPECT_TRUE(table_view()->GetEnabled());
  EXPECT_FALSE(IsDeviceSelected());

  AddUnpairedDevice();
  EXPECT_EQ(2, table_view()->GetRowCount());
  EXPECT_EQ(GetUnpairedDeviceTextAtRow(1), table_model()->GetText(1, 0));
  EXPECT_TRUE(table_view()->GetEnabled());
  EXPECT_FALSE(IsDeviceSelected());
}

TEST_F(DeviceChooserContentViewTest, RemoveOption) {
  // Called from TableView::OnItemsRemoved().
  EXPECT_CALL(table_observer(), OnSelectionChanged()).Times(3);
  AddPairedDevice();
  AddUnpairedDevice();
  AddUnpairedDevice();

  // Remove the paired device.
  controller()->RemoveDevice(0);
  EXPECT_EQ(2, table_view()->GetRowCount());
  EXPECT_EQ(GetUnpairedDeviceTextAtRow(0), table_model()->GetText(0, 0));
  EXPECT_EQ(GetUnpairedDeviceTextAtRow(1), table_model()->GetText(1, 0));
  EXPECT_TRUE(table_view()->GetEnabled());
  EXPECT_FALSE(IsDeviceSelected());

  // Remove everything.
  controller()->RemoveDevice(1);
  controller()->RemoveDevice(0);
  // Should be back to the initial state.
  ExpectNoDevices();
}

TEST_F(DeviceChooserContentViewTest, UpdateOption) {
  EXPECT_CALL(table_observer(), OnSelectionChanged()).Times(0);
  AddPairedDevice();
  AddUnpairedDevice();
  AddUnpairedDevice();

  EXPECT_EQ(GetUnpairedDeviceTextAtRow(1), table_model()->GetText(1, 0));
  controller()->UpdateDevice(
      1, {"Nice Device", FakeBluetoothChooserController::CONNECTED,
          FakeBluetoothChooserController::PAIRED,
          FakeBluetoothChooserController::kSignalStrengthUnknown});
  EXPECT_EQ(3, table_view()->GetRowCount());
  EXPECT_EQ(GetPairedDeviceTextAtRow(1), table_model()->GetText(1, 0));
  EXPECT_FALSE(IsDeviceSelected());
}

TEST_F(DeviceChooserContentViewTest, SelectAndDeselectAnOption) {
  EXPECT_CALL(table_observer(), OnSelectionChanged()).Times(2);
  AddPairedDevice();
  AddUnpairedDevice();

  table_view()->Select(0);
  EXPECT_TRUE(IsDeviceSelected());
  EXPECT_EQ(0, table_view()->GetFirstSelectedRow());

  table_view()->Select(-1);
  EXPECT_FALSE(IsDeviceSelected());
  EXPECT_EQ(-1, table_view()->GetFirstSelectedRow());
}

TEST_F(DeviceChooserContentViewTest, TurnBluetoothOffAndOn) {
  AddUnpairedDevice();
  controller()->SetBluetoothStatus(
      FakeBluetoothChooserController::BluetoothStatus::UNAVAILABLE);

  EXPECT_FALSE(table_parent()->GetVisible());
  EXPECT_FALSE(no_options_view()->GetVisible());
  EXPECT_TRUE(adapter_off_view()->GetVisible());
  EXPECT_FALSE(throbber()->GetVisible());
  EXPECT_FALSE(scanning_label()->GetVisible());
  EXPECT_TRUE(re_scan_button()->GetVisible());
  EXPECT_FALSE(re_scan_button()->GetEnabled());

  controller()->RemoveDevice(0);
  controller()->SetBluetoothStatus(
      FakeBluetoothChooserController::BluetoothStatus::IDLE);
  ExpectNoDevices();
  EXPECT_FALSE(adapter_off_view()->GetVisible());
  EXPECT_FALSE(throbber()->GetVisible());
  EXPECT_FALSE(scanning_label()->GetVisible());
  EXPECT_TRUE(re_scan_button()->GetVisible());
  EXPECT_TRUE(re_scan_button()->GetEnabled());
}

TEST_F(DeviceChooserContentViewTest, ScanForDevices) {
  controller()->SetBluetoothStatus(
      FakeBluetoothChooserController::BluetoothStatus::SCANNING);
  EXPECT_EQ(0, table_view()->GetRowCount());
  EXPECT_FALSE(table_view()->GetEnabled());
  EXPECT_FALSE(adapter_off_view()->GetVisible());
  EXPECT_TRUE(throbber()->GetVisible());
  EXPECT_TRUE(scanning_label()->GetVisible());
  EXPECT_FALSE(re_scan_button()->GetVisible());

  AddUnpairedDevice();
  EXPECT_EQ(1, table_view()->GetRowCount());
  EXPECT_TRUE(table_view()->GetEnabled());
  EXPECT_FALSE(adapter_off_view()->GetVisible());
  EXPECT_TRUE(throbber()->GetVisible());
  EXPECT_TRUE(scanning_label()->GetVisible());
  EXPECT_FALSE(IsDeviceSelected());
  EXPECT_FALSE(re_scan_button()->GetVisible());
}

TEST_F(DeviceChooserContentViewTest, ClickAdapterOffHelpLink) {
  EXPECT_CALL(*controller(), OpenAdapterOffHelpUrl()).Times(1);
  static_cast<views::StyledLabel*>(adapter_off_view()->children().front())
      ->LinkClicked(nullptr, 0);
}

TEST_F(DeviceChooserContentViewTest, ClickRescanButton) {
  EXPECT_CALL(*controller(), RefreshOptions()).Times(1);
  const gfx::Point point(10, 10);
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, point, point,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  content_view().ButtonPressed(re_scan_button(), event);
}

TEST_F(DeviceChooserContentViewTest, ClickHelpButton) {
  EXPECT_CALL(*controller(), OpenHelpCenterUrl()).Times(1);
  // The content view doesn't have a direct reference to the help button, so we
  // need to find it. It's on the left (in LTR) so it should be the first child.
  auto* help_button = static_cast<views::ImageButton*>(
      extra_views_container_->children().front());
  const gfx::Point point(10, 10);
  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, point, point,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  content_view().ButtonPressed(help_button, event);
}

TEST_F(DeviceChooserContentViewTest, SetTableViewAlwaysDisabled) {
  controller()->set_table_view_always_disabled(true);
  EXPECT_FALSE(table_view()->GetEnabled());
  AddUnpairedDevice();
  EXPECT_EQ(1, table_view()->GetRowCount());
  // The table should still be disabled even though there's an option.
  EXPECT_FALSE(table_view()->GetEnabled());
}
