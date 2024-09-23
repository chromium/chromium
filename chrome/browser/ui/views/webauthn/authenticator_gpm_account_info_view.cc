// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_gpm_account_info_view.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"

namespace {
constexpr int kBetweenChildSpacing = 12;
constexpr int kHorizontalInset = 16;
constexpr int kVerticalInset = 12;
}  // namespace

BEGIN_METADATA(AuthenticatorGpmAccountInfoView)
END_METADATA

AuthenticatorGpmAccountInfoView::AuthenticatorGpmAccountInfoView(
    AuthenticatorGpmPinSheetModelBase* sheet_model) {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_inside_border_insets(
      gfx::Insets::VH(kVerticalInset, kHorizontalInset));
  layout->set_between_child_spacing(kBetweenChildSpacing);

  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetImage(sheet_model->GetGpmAccountImage().AsImageSkia());
  AddChildView(std::move(image_view));

  auto label_column = std::make_unique<views::BoxLayoutView>();
  label_column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  label_column->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  label_column->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  label_column->AddChildView(std::make_unique<views::Label>(
      sheet_model->GetGpmAccountName(), views::style::CONTEXT_LABEL,
      views::style::STYLE_BODY_4_BOLD));
  auto* email_label = label_column->AddChildView(
      std::make_unique<views::Label>(sheet_model->GetGpmAccountEmail()));
  email_label->SetElideBehavior(gfx::ElideBehavior::ELIDE_EMAIL);
  AddChildView(std::move(label_column));
}

AuthenticatorGpmAccountInfoView::~AuthenticatorGpmAccountInfoView() = default;
