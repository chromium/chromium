// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_VIEWS_LOGIN_TEXTFIELD_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_VIEWS_LOGIN_TEXTFIELD_H_

#include "ash/style/system_textfield.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

// A textfield that selects all text on focus and allows to switch between
// show/hide password modes.
class LoginTextfield : public SystemTextfield {
  METADATA_HEADER(LoginTextfield, SystemTextfield)
 public:
  class Delegate {
   public:
    virtual void OnTextfieldBlur() {}
    virtual void OnTextfieldFocus() {}
  };

  LoginTextfield();
  LoginTextfield(const LoginTextfield&) = delete;
  LoginTextfield& operator=(const LoginTextfield&) = delete;
  ~LoginTextfield() override;

  // views::Textfield:
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void OnBlur() override;
  void OnFocus() override;

  // This is useful when the display password button is not shown. In such a
  // case, the login text field needs to define its size.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  void SetDelegate(Delegate* delegate);

 private:
  raw_ptr<Delegate> delegate_ = nullptr;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_IMPL_VIEWS_LOGIN_TEXTFIELD_H_
