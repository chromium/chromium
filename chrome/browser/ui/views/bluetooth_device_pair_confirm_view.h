// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BLUETOOTH_DEVICE_PAIR_CONFIRM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BLUETOOTH_DEVICE_PAIR_CONFIRM_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/window/dialog_delegate.h"

// A dialog allowing the user to enter Bluetooth credentials (i.e. a PIN).
class BluetoothDevicePairConfirmView : public views::DialogDelegateView,
                                       public views::TextfieldController {
 public:
  METADATA_HEADER(BluetoothDevicePairConfirmView);
  BluetoothDevicePairConfirmView(
      const std::u16string& device_identifier,
      content::BluetoothDelegate::PairConfirmCallback close_callback);
  BluetoothDevicePairConfirmView(const BluetoothDevicePairConfirmView&) =
      delete;
  BluetoothDevicePairConfirmView& operator=(
      const BluetoothDevicePairConfirmView&) = delete;
  ~BluetoothDevicePairConfirmView() override;

  // Initialize the controls on the dialog.
  void InitControls(const std::u16string& device_identifier);

  // View:
  gfx::Size CalculatePreferredSize() const override;

  std::u16string GetWindowTitle() const override;

 private:
  void OnDialogAccepted();

  content::BluetoothDelegate::PairConfirmCallback close_callback_;
  raw_ptr<views::Textfield> passkey_text_ = nullptr;
  raw_ptr<views::View> icon_view_ = nullptr;
  raw_ptr<views::View> contents_wrapper_ = nullptr;
  base::WeakPtrFactory<BluetoothDevicePairConfirmView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BLUETOOTH_DEVICE_PAIR_CONFIRM_VIEW_H_
