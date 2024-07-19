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
constexpr int kHorizontalPadding = 4;
}  // namespace

BEGIN_METADATA(AuthenticatorGpmAccountInfoView)
END_METADATA

AuthenticatorGpmAccountInfoView::AuthenticatorGpmAccountInfoView(
    AuthenticatorGpmPinSheetModelBase* sheet_model) {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  row_ = std::make_unique<views::BoxLayoutView>();
  row_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  row_->SetBetweenChildSpacing(kHorizontalPadding);
  auto image_view = std::make_unique<views::ImageView>();
  image_view->SetImage(sheet_model->GetGpmAccountImage().AsImageSkia());
  row_->AddChildView(std::move(image_view));
  row_->AddChildView(
      std::make_unique<views::Label>(sheet_model->GetGpmAccountEmail()));
  AddChildView(row_.get());
}

AuthenticatorGpmAccountInfoView::~AuthenticatorGpmAccountInfoView() = default;
