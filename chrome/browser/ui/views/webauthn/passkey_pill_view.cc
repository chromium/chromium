// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/passkey_pill_view.h"

#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"

namespace {
std::u16string GetUserNameForDisplay(
    const device::PublicKeyCredentialUserEntity& user) {
  if (!user.name || user.name->empty()) {
    return l10n_util::GetStringUTF16(IDS_WEBAUTHN_UNKNOWN_ACCOUNT);
  }
  return base::UTF8ToUTF16(*user.name);
}
}  // namespace

PasskeyPillView::PasskeyPillView(
    const device::PublicKeyCredentialUserEntity& user) {
  constexpr size_t kVerticalMargin = 14, kHorizontalMargin = 24,
                   kPillHeight = 63;

  auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  layout->SetMinimumCrossAxisSize(kPillHeight);

  // Force 16px margin between icon and label.
  layout->SetDefault(views::kMarginsKey, gfx::Insets::VH(0, 16));
  layout->SetInteriorMargin(
      gfx::Insets::VH(kVerticalMargin, kHorizontalMargin));
  layout->SetCollapseMargins(true);

  AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kPasskeyIcon, ui::kColorAccent,
          /*icon_size=*/24)));

  auto* label = AddChildView(std::make_unique<views::Label>(
      GetUserNameForDisplay(user), views::style::CONTEXT_DIALOG_BODY_TEXT));

  // Make the username label elide with appropriate behavior.
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero));
  label->SetElideBehavior(gfx::ELIDE_EMAIL);

  SetBorder(views::CreateThemedRoundedRectBorder(
      /*thickness=*/1, /*corner_radius=*/16, ui::kColorSeparator));
}

BEGIN_METADATA(PasskeyPillView, views::View)
END_METADATA
