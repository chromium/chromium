// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DEVICE_CHOOSER_CONTENT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DEVICE_CHOOSER_CONTENT_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "ui/base/models/table_model.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"

class BluetoothStatusContainer;
namespace views {
class Label;
class LabelButton;
class TableView;
class TableViewObserver;
class Throbber;
}

// A bubble or dialog view for choosing among several options in a table.
// Used for WebUSB/WebBluetooth device selection for Chrome and extensions.
class DeviceChooserContentView : public views::View,
                                 public ui::TableModel,
                                 public ChooserController::View,
                                 public views::StyledLabelListener,
                                 public views::ButtonListener {
 public:
  DeviceChooserContentView(
      views::TableViewObserver* table_view_observer,
      std::unique_ptr<ChooserController> chooser_controller);
  ~DeviceChooserContentView() override;

  // views::View:
  gfx::Size GetMinimumSize() const override;

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int row, int column_id) override;
  void SetObserver(ui::TableModelObserver* observer) override;
  gfx::ImageSkia GetIcon(int row) override;

  // ChooserController::View:
  void OnOptionsInitialized() override;
  void OnOptionAdded(size_t index) override;
  void OnOptionRemoved(size_t index) override;
  void OnOptionUpdated(size_t index) override;
  void OnAdapterEnabledChanged(bool enabled) override;
  void OnRefreshStateChanged(bool refreshing) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  base::string16 GetWindowTitle() const;
  std::unique_ptr<views::View> CreateExtraView();
  bool IsDialogButtonEnabled(ui::DialogButton button) const;
  void Accept();
  void Cancel();
  void Close();
  void UpdateTableView();

  // Test-only accessors to children.
  views::TableView* table_view_for_testing() { return table_view_; }
  views::LabelButton* ReScanButtonForTesting();
  views::Throbber* ThrobberForTesting();
  views::Label* ScanningLabelForTesting();

 private:
  friend class DeviceChooserContentViewTest;

  std::unique_ptr<ChooserController> chooser_controller_;

  bool adapter_enabled_ = true;

  views::View* table_parent_ = nullptr;
  views::TableView* table_view_ = nullptr;
  views::View* no_options_view_ = nullptr;
  views::View* adapter_off_view_ = nullptr;
  BluetoothStatusContainer* bluetooth_status_container_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DeviceChooserContentView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DEVICE_CHOOSER_CONTENT_VIEW_H_
