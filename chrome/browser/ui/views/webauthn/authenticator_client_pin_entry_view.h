// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CLIENT_PIN_ENTRY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CLIENT_PIN_ENTRY_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class Label;
class Textfield;
}  // namespace views

// View showing a label and text field for entering an authenticator PIN.
//
// TODO(martinkr): Reuse for BLE PIN or fold into
// AuthenticatorClientPinEntrySheetModel.
class AuthenticatorClientPinEntryView : public views::View,
                                        public views::TextfieldController {
  METADATA_HEADER(AuthenticatorClientPinEntryView, views::View)

 public:
  class Delegate {
   public:
    virtual void OnPincodeChanged(std::u16string pin_code) = 0;
    virtual void OnConfirmationChanged(std::u16string pin_confirmation) = 0;
  };

  explicit AuthenticatorClientPinEntryView(Delegate* delegate,
                                           bool show_confirmation_text_field);
  AuthenticatorClientPinEntryView(const AuthenticatorClientPinEntryView&) =
      delete;
  AuthenticatorClientPinEntryView& operator=(
      const AuthenticatorClientPinEntryView&) = delete;
  ~AuthenticatorClientPinEntryView() override;

 private:
  // views::View:
  void RequestFocus() override;
  void OnThemeChanged() override;

  // views::TextFieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  const raw_ptr<Delegate> delegate_;
  raw_ptr<views::Label> pin_label_ = nullptr;
  raw_ptr<views::Label> confirmation_label_ = nullptr;
  raw_ptr<views::Textfield> pin_text_field_ = nullptr;
  raw_ptr<views::Textfield> confirmation_text_field_ = nullptr;
  const bool show_confirmation_text_field_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_CLIENT_PIN_ENTRY_VIEW_H_
