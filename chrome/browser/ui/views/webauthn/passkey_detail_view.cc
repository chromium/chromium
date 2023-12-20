// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_detail_view.h"

#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

namespace {
std::u16string GetUserNameForDisplay(
    const device::PublicKeyCredentialUserEntity& user) {
  if (!user.name || user.name->empty()) {
    return l10n_util::GetStringUTF16(IDS_WEBAUTHN_UNKNOWN_ACCOUNT);
  }
  return base::UTF8ToUTF16(*user.name);
}
}  // namespace

PasskeyDetailView::PasskeyDetailView(
    const device::PublicKeyCredentialUserEntity& user) {
  constexpr size_t kHeight = 40, kMargin = 16;

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_minimum_cross_axis_size(kHeight);
  layout->set_between_child_spacing(kMargin);

  AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kPasskeyIcon, ui::kColorAccent,
          /*icon_size=*/24)));

  auto* label = AddChildView(std::make_unique<views::Label>(
      GetUserNameForDisplay(user), views::style::CONTEXT_DIALOG_BODY_TEXT));
  // Make the username label elide with appropriate behavior. Since this is
  // website-controlled data inside browser UI, limit the width to something
  // reasonable.
  label->SetMaximumWidthSingleLine(300);
  label->SetElideBehavior(gfx::ELIDE_EMAIL);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  layout->SetFlexForView(label, 1);
}

void PasskeyDetailView::OnThemeChanged() {
  View::OnThemeChanged();
  SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets::TLBR(1, 0, 1, 0),
      GetColorProvider()->GetColor(ui::kColorSeparator)));
}

BEGIN_METADATA(PasskeyDetailView)
END_METADATA
