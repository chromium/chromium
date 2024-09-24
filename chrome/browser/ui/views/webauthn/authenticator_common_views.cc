// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_common_views.h"

#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

namespace {
constexpr int kGapSize = 8;
constexpr int kGpmIconSize = 20;
constexpr int kMediumIconSize = 26;
constexpr int kHorizontalInset = 8;
constexpr int kHorizontalSpacing = 16;
}  // namespace

std::unique_ptr<views::View> CreatePasskeyWithUsernameLabel(
    std::u16string username) {
  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  container->SetInsideBorderInsets(gfx::Insets::VH(0, kHorizontalInset));
  container->SetBetweenChildSpacing(kHorizontalSpacing);
  container->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          vector_icons::kPasskeyIcon, ui::kColorIcon, kMediumIconSize)));

  auto username_column = std::make_unique<views::BoxLayoutView>();
  username_column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  username_column->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  auto* username_view =
      username_column->AddChildView(std::make_unique<views::Label>(
          username, views::style::CONTEXT_DIALOG_BODY_TEXT));
  username_column->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_PASSKEY),
      views::style::CONTEXT_DIALOG_BODY_TEXT));
  username_view->SetElideBehavior(gfx::ElideBehavior::ELIDE_EMAIL);

  container->AddChildView(std::move(username_column));
  return std::move(container);
}

std::unique_ptr<views::View> CreateGpmIconWithLabel() {
  auto view = std::make_unique<views::BoxLayoutView>();
  view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  view->SetBetweenChildSpacing(kGapSize);
  view->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          GooglePasswordManagerVectorIcon(), ui::kColorIcon, kGpmIconSize)));
  view->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_SOURCE_GOOGLE_PASSWORD_MANAGER)));
  return view;
}
