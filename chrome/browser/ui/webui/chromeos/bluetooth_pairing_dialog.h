// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_BLUETOOTH_PAIRING_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_BLUETOOTH_PAIRING_DIALOG_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

class BluetoothPairingDialog : public SystemWebDialogDelegate {
 public:
  // Shows a bluetooth pairing dialog. The dialog is returned for testing.
  static SystemWebDialogDelegate* ShowDialog(
      const std::string& address,
      const std::u16string& name_for_display,
      bool paired,
      bool connected);

 protected:
  BluetoothPairingDialog(const std::string& address,
                         const std::u16string& name_for_display,
                         bool paired,
                         bool connected);
  ~BluetoothPairingDialog() override;

  // SystemWebDialogDelegate
  const std::string& Id() override;

  // ui::WebDialogDelegate
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;

 private:
  std::string address_;
  base::DictionaryValue device_data_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothPairingDialog);
};

// A WebUI to host bluetooth device pairing web ui.
class BluetoothPairingDialogUI : public ui::WebDialogUI {
 public:
  explicit BluetoothPairingDialogUI(content::WebUI* web_ui);
  ~BluetoothPairingDialogUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothPairingDialogUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_BLUETOOTH_PAIRING_DIALOG_H_
