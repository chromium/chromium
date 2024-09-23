// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BLUETOOTH_DEVICE_PAIR_CONFIRM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BLUETOOTH_DEVICE_PAIR_CONFIRM_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "ui/views/window/dialog_delegate.h"

// A dialog allowing the user to confirm Bluetooth device pairing with or
// without PIN being displayed.
class BluetoothDevicePairConfirmView : public views::DialogDelegateView {
  METADATA_HEADER(BluetoothDevicePairConfirmView, views::DialogDelegateView)

 public:
  BluetoothDevicePairConfirmView(
      const std::u16string& device_identifier,
      const std::optional<std::u16string>& pin,
      content::BluetoothDelegate::PairPromptCallback close_callback);
  BluetoothDevicePairConfirmView(const BluetoothDevicePairConfirmView&) =
      delete;
  BluetoothDevicePairConfirmView& operator=(
      const BluetoothDevicePairConfirmView&) = delete;
  ~BluetoothDevicePairConfirmView() override;

  // Initialize the controls on the dialog.
  void InitControls(const std::u16string& device_identifier,
                    const std::optional<std::u16string>& pin);

  // View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // WidgetDelegate:
  std::u16string GetWindowTitle() const override;

 private:
  // Runs the |close_callback_| with the PairPromptResult if the dialog is
  // accepted.
  void OnDialogAccepted();

  content::BluetoothDelegate::PairPromptCallback close_callback_;
  raw_ptr<views::View> icon_view_ = nullptr;
  raw_ptr<views::View> contents_wrapper_ = nullptr;
  bool display_pin_ = false;
  base::WeakPtrFactory<BluetoothDevicePairConfirmView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BLUETOOTH_DEVICE_PAIR_CONFIRM_VIEW_H_
