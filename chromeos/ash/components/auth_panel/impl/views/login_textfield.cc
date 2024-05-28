// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/auth_panel/impl/views/login_textfield.h"

#include "ash/style/system_textfield.h"
#include "ash/style/typography.h"
#include "chromeos/ash/components/auth_panel/impl/views/auth_panel_views_utils.h"
#include "chromeos/ash/components/auth_panel/impl/views/view_size_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"

namespace ash {

LoginTextfield::LoginTextfield() : SystemTextfield(Type::kMedium) {
  const gfx::FontList font_list =
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          TypographyToken::kLegacyBody1);

  SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  set_placeholder_font_list(font_list);
  SetFontList(font_list);
  SetObscuredGlyphSpacing(kPasswordGlyphSpacing);
  // Remove focus ring to remain consistent with other implementations of
  // login input fields.
  views::FocusRing::Remove(this);
  SetShowBackground(false);
  SetBackgroundEnabled(false);
  ConfigureAuthTextField(this);
}

LoginTextfield::~LoginTextfield() = default;

void LoginTextfield::AboutToRequestFocusFromTabTraversal(bool reverse) {
  if (!GetText().empty()) {
    SelectAll(/*reversed=*/false);
  }
}

void LoginTextfield::OnBlur() {
  SystemTextfield::OnBlur();
  CHECK(delegate_);
  delegate_->OnTextfieldBlur();
}

void LoginTextfield::OnFocus() {
  SystemTextfield::OnFocus();
  CHECK(delegate_);
  delegate_->OnTextfieldFocus();
}

gfx::Size LoginTextfield::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kPasswordTotalWidthDp, kIconSizeDp);
}

void LoginTextfield::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

BEGIN_METADATA(LoginTextfield)
END_METADATA

}  // namespace ash
