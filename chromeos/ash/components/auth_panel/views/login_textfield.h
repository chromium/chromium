// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_VIEWS_LOGIN_TEXTFIELD_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_VIEWS_LOGIN_TEXTFIELD_H_

#include "ash/style/system_textfield.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/auth_panel/auth_factor_store.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

class AuthPanelEventDispatcher;
class AuthFactorStore;

// A textfield that selects all text on focus and allows to switch between
// show/hide password modes.
class LoginTextfield : public SystemTextfield {
  METADATA_HEADER(LoginTextfield, SystemTextfield)

 public:
  explicit LoginTextfield(AuthPanelEventDispatcher* dispatcher);
  LoginTextfield(const LoginTextfield&) = delete;
  LoginTextfield& operator=(const LoginTextfield&) = delete;
  ~LoginTextfield() override = default;

  // views::Textfield:
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void OnBlur() override;
  void OnFocus() override;
  // This is useful when the display password button is not shown. In such a
  // case, the login text field needs to define its size.
  gfx::Size CalculatePreferredSize() const override;

  void OnStateChanged(
      const AuthFactorStore::State::PasswordViewState& password_view_state);

 private:
  raw_ptr<AuthPanelEventDispatcher> dispatcher_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_VIEWS_LOGIN_TEXTFIELD_H_
