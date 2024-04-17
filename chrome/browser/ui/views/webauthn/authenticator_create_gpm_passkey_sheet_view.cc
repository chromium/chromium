// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_create_gpm_passkey_sheet_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/style/typography.h"

namespace {
constexpr int kPasskeyIconSize = 20;
}  // namespace

AuthenticatorCreateGpmPasskeySheetView::AuthenticatorCreateGpmPasskeySheetView(
    std::unique_ptr<AuthenticatorCreateGpmPasskeySheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorCreateGpmPasskeySheetView::
    ~AuthenticatorCreateGpmPasskeySheetView() = default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorCreateGpmPasskeySheetView::AutoFocus>
AuthenticatorCreateGpmPasskeySheetView::BuildStepSpecificContent() {
  auto* sheet_model =
      static_cast<AuthenticatorCreateGpmPasskeySheetModel*>(model());
  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  container->SetBetweenChildSpacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  container->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kPasskeyIcon, ui::kColorIcon, kPasskeyIconSize)));

  auto username_column = std::make_unique<views::BoxLayoutView>();
  username_column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  username_column->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  std::string username =
      sheet_model->dialog_model()->user_entity.name.value_or("");
  auto* username_view =
      username_column->AddChildView(std::make_unique<views::Label>(
          base::UTF8ToUTF16(username), views::style::CONTEXT_DIALOG_BODY_TEXT));
  username_column->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_PASSKEY),
      views::style::CONTEXT_DIALOG_BODY_TEXT));
  username_view->SetElideBehavior(gfx::ElideBehavior::ELIDE_EMAIL);

  container->AddChildView(std::move(username_column));
  return std::make_pair(std::move(container), AutoFocus::kNo);
}
