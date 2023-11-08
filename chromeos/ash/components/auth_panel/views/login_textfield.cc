// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auth_panel/views/login_textfield.h"

#include "ash/style/system_textfield.h"
#include "ash/style/typography.h"
#include "chromeos/ash/components/auth_panel/auth_panel_event_dispatcher.h"
#include "chromeos/ash/components/auth_panel/views/auth_panel_views_utils.h"
#include "chromeos/ash/components/auth_panel/views/view_size_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/views/border.h"

namespace ash {

LoginTextfield::LoginTextfield(AuthPanelEventDispatcher* dispatcher)
    : SystemTextfield(Type::kMedium), dispatcher_(dispatcher) {
  const gfx::FontList font_list =
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          TypographyToken::kLegacyBody1);

  SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  set_placeholder_font_list(font_list);
  SetFontList(font_list);
  SetObscuredGlyphSpacing(kPasswordGlyphSpacing);
  ConfigureAuthTextField(this);
}

void LoginTextfield::AboutToRequestFocusFromTabTraversal(bool reverse) {
  if (!GetText().empty()) {
    SelectAll(/*reversed=*/false);
  }
}

void LoginTextfield::OnBlur() {
  dispatcher_->DispatchEvent(AuthPanelEventDispatcher::UserAction{
      AuthPanelEventDispatcher::UserAction::Type::kPasswordTextfieldBlurred,
      absl::nullopt});
  SystemTextfield::OnBlur();
}

void LoginTextfield::OnFocus() {
  SystemTextfield::OnFocus();
  dispatcher_->DispatchEvent(AuthPanelEventDispatcher::UserAction{
      AuthPanelEventDispatcher::UserAction::Type::kPasswordTextfieldFocused,
      absl::nullopt});
}

gfx::Size LoginTextfield::CalculatePreferredSize() const {
  return gfx::Size(kPasswordTotalWidthDp, kIconSizeDp);
}

void LoginTextfield::OnStateChanged(
    const AuthFactorStore::State::PasswordViewState& password_view_state) {
  SetReadOnly(!password_view_state.is_factor_enabled_);
  SetCursorEnabled(password_view_state.is_factor_enabled_);

  SetTextInputType(password_view_state.is_password_visible_
                       ? ui::TEXT_INPUT_TYPE_NULL
                       : ui::TEXT_INPUT_TYPE_PASSWORD);
}

BEGIN_METADATA(LoginTextfield)
END_METADATA

}  // namespace ash
