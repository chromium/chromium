// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BLUETOOTH_DEVICE_CREDENTIALS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_BLUETOOTH_DEVICE_CREDENTIALS_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/window/dialog_delegate.h"

// A dialog allowing the user to enter Bluetooth credentials (i.e. a PIN).
class BluetoothDeviceCredentialsView : public views::DialogDelegateView,
                                       public views::TextfieldController {
  METADATA_HEADER(BluetoothDeviceCredentialsView, views::DialogDelegateView)

 public:
  BluetoothDeviceCredentialsView(
      const std::u16string& device_identifier,
      content::BluetoothDelegate::PairPromptCallback close_callback);
  BluetoothDeviceCredentialsView(const BluetoothDeviceCredentialsView&) =
      delete;
  BluetoothDeviceCredentialsView& operator=(
      const BluetoothDeviceCredentialsView&) = delete;
  ~BluetoothDeviceCredentialsView() override;

  // Initialize the controls on the dialog.
  void InitControls(const std::u16string& device_identifier);

  // View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // DialogDelegateView:
  bool IsDialogButtonEnabled(ui::mojom::DialogButton button) const override;
  std::u16string GetWindowTitle() const override;

  // WidgetDelegate:
  views::View* GetInitiallyFocusedView() override;

 private:
  // Runs the |close_callback_| with the PairPromptResult if the dialog is
  // accepted.
  void OnDialogAccepted();
  // TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  content::BluetoothDelegate::PairPromptCallback close_callback_;
  raw_ptr<views::Textfield> passkey_text_ = nullptr;
  raw_ptr<views::View> icon_view_ = nullptr;
  raw_ptr<views::View> contents_wrapper_ = nullptr;
  base::WeakPtrFactory<BluetoothDeviceCredentialsView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BLUETOOTH_DEVICE_CREDENTIALS_VIEW_H_
