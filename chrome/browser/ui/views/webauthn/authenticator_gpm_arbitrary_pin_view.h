// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_ARBITRARY_PIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_ARBITRARY_PIN_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {
class Textfield;
class ToggleImageButton;
}  // namespace views

// View showing a textfield for entering an alphanumeric GPM pin.
class AuthenticatorGPMArbitraryPinView : public views::View,
                                         public views::TextfieldController {
  METADATA_HEADER(AuthenticatorGPMArbitraryPinView, views::View)

 public:
  class Delegate {
   public:
    virtual void OnPinChanged(std::u16string pin) = 0;
  };

  explicit AuthenticatorGPMArbitraryPinView(
      bool ui_disabled,
      const std::u16string& pin,
      const std::u16string& pin_accessible_name,
      const std::u16string& pin_accessible_description,
      Delegate* delegate);

  AuthenticatorGPMArbitraryPinView(const AuthenticatorGPMArbitraryPinView&) =
      delete;
  AuthenticatorGPMArbitraryPinView& operator=(
      const AuthenticatorGPMArbitraryPinView&) = delete;
  ~AuthenticatorGPMArbitraryPinView() override;

 private:
  void OnRevealButtonClicked();

  // views::View:
  void RequestFocus() override;

  // views::TextFieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  const raw_ptr<Delegate> delegate_;
  raw_ptr<views::Textfield> pin_textfield_ = nullptr;
  raw_ptr<views::ToggleImageButton> reveal_button_ = nullptr;
  bool pin_revealed_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_GPM_ARBITRARY_PIN_VIEW_H_
