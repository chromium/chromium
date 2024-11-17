// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_qr_sheet_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/webauthn/authenticator_qr_centered_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace {

constexpr int kSecurityKeyIconSize = 30;

}  // namespace

AuthenticatorQRSheetView::AuthenticatorQRSheetView(
    std::unique_ptr<AuthenticatorQRSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)),
      qr_string_(*static_cast<AuthenticatorQRSheetModel*>(model())
                      ->dialog_model()
                      ->cable_qr_string) {}

AuthenticatorQRSheetView::~AuthenticatorQRSheetView() = default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorQRSheetView::BuildStepSpecificContent() {
  auto* sheet_model = static_cast<AuthenticatorQRSheetModel*>(model());
  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetBetweenChildSpacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  container->AddChildView(
      std::make_unique<AuthenticatorQrCenteredView>(qr_string_));

  const std::vector<std::u16string> labels =
      sheet_model->GetSecurityKeyLabels();
  if (!labels.empty()) {
    auto* security_key_container =
        container->AddChildView(std::make_unique<views::TableLayoutView>());
    security_key_container->AddColumn(
        views::LayoutAlignment::kStretch, views::LayoutAlignment::kStretch,
        views::TableLayout::kFixedSize,
        views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
    security_key_container->AddPaddingColumn(
        views::TableLayout::kFixedSize,
        views::LayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_LABEL_HORIZONTAL));
    security_key_container->AddColumn(
        views::LayoutAlignment::kStretch, views::LayoutAlignment::kStretch,
        /*horizontal_resize=*/1, views::TableLayout::ColumnSize::kUsePreferred,
        0, 0);
    security_key_container->AddRows(labels.size(),
                                    views::TableLayout::kFixedSize);
    security_key_container->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            kUsbSecurityKeyIcon, ui::kColorIcon, kSecurityKeyIconSize)));
    auto* label_container = security_key_container->AddChildView(
        std::make_unique<views::BoxLayoutView>());
    label_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
    label_container->SetBetweenChildSpacing(
        views::LayoutProvider::Get()->GetDistanceMetric(
            views::DISTANCE_RELATED_CONTROL_VERTICAL));

    for (const std::u16string& label_str : labels) {
      auto* label =
          label_container->AddChildView(std::make_unique<views::Label>(
              label_str, views::style::CONTEXT_DIALOG_BODY_TEXT));
      label->SetMultiLine(true);
      label->SetAllowCharacterBreak(true);
      label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    }
  }
  return std::make_pair(std::move(container), AutoFocus::kNo);
}

BEGIN_METADATA(AuthenticatorQRSheetView)
END_METADATA
