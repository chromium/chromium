// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_BLE_PIN_ENTRY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_BLE_PIN_ENTRY_VIEW_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class Textfield;
}  // namespace views

// View that shows a label and a textfield to which user will provide pincode to
// pair with Bluetooth authenticator.
class BlePinEntryView : public views::View, public views::TextfieldController {
 public:
  // Interface that the client should implement to learn BLE pincode user
  // provided via the textfield input.
  class Delegate {
   public:
    virtual void OnPincodeChanged(base::string16 pincode) = 0;
  };

  explicit BlePinEntryView(Delegate* delegate);
  ~BlePinEntryView() override;

 private:
  // views::View:
  void RequestFocus() override;

  // views::TextFieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  Delegate* const delegate_;
  views::Textfield* pin_text_field_;

  DISALLOW_COPY_AND_ASSIGN(BlePinEntryView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_BLE_PIN_ENTRY_VIEW_H_
