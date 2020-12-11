// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CLIENT_PIN_ENTRY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CLIENT_PIN_ENTRY_VIEW_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class Textfield;
}  // namespace views

// View showing a label and text field for entering an authenticator PIN.
//
// TODO(martinkr): Reuse for BLE PIN or fold into
// AuthenticatorClientPinEntrySheetModel.
class AuthenticatorClientPinEntryView : public views::View,
                                        public views::TextfieldController {
 public:
  class Delegate {
   public:
    virtual void OnPincodeChanged(base::string16 pin_code) = 0;
    virtual void OnConfirmationChanged(base::string16 pin_confirmation) = 0;
  };

  explicit AuthenticatorClientPinEntryView(Delegate* delegate,
                                           bool show_confirmation_text_field);
  ~AuthenticatorClientPinEntryView() override;

 private:
  // views::View:
  void RequestFocus() override;

  // views::TextFieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  Delegate* const delegate_;
  views::Textfield* pin_text_field_ = nullptr;
  views::Textfield* confirmation_text_field_ = nullptr;
  const bool show_confirmation_text_field_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorClientPinEntryView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CLIENT_PIN_ENTRY_VIEW_H_
