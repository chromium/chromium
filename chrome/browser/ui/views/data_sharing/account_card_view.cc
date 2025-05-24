// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/data_sharing/account_card_view.h"

#include "base/strings/utf_string_conversions.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kAccountImageSize = 40;
constexpr int kAccountImageMargin = 16;

// Resize and crop image into a circle.
ui::ImageModel GetResizedImage(const gfx::Image& image) {
  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
          kAccountImageSize / 2.0,
          gfx::ResizedImage(image,
                            gfx::Size(kAccountImageSize, kAccountImageSize))
              .AsImageSkia()));
}
}  // namespace

AccountCardView::AccountCardView(AccountInfo account_info) {
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::LayoutOrientation::kHorizontal))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  GetViewAccessibility().SetRole(ax::mojom::Role::kRow);

  // Hide AccountCardView if account info is empty.
  SetVisible(!account_info.IsEmpty());

  if (!account_info.IsEmpty()) {
    AddChildView(std::make_unique<views::ImageView>(
                     GetResizedImage(account_info.account_image)))
        ->SetProperty(views::kMarginsKey,
                      gfx::Insets::TLBR(0, 0, 0, kAccountImageMargin));

    auto* label_container = AddChildView(std::make_unique<views::View>());
    label_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::LayoutOrientation::kVertical));

    auto* name =
        label_container->AddChildView(std::make_unique<views::Label>());
    name->SetText(base::UTF8ToUTF16(account_info.full_name));
    name->SetTextStyle(views::style::TextStyle::STYLE_BODY_3);
    name->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

    auto* email =
        label_container->AddChildView(std::make_unique<views::Label>());
    email->SetText(base::UTF8ToUTF16(account_info.email));
    email->SetTextStyle(views::style::TextStyle::STYLE_BODY_4);
    email->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    email->SetEnabledColor(ui::kColorSysOnSurfaceSubtle);
  }
}

AccountCardView::~AccountCardView() = default;

BEGIN_METADATA(AccountCardView)
END_METADATA
