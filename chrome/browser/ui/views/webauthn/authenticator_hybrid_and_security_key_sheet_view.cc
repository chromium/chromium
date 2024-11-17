// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_hybrid_and_security_key_sheet_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/webauthn/authenticator_qr_centered_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "device/fido/fido_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace {

constexpr int kIconSize = 20;
constexpr int kIconAndTextGap = 8;

std::unique_ptr<views::View> CreateMechanismDescriptionWithIcon(
    const gfx::VectorIcon& vector_icon,
    const std::u16string& header_text,
    const std::vector<std::u16string>& descriptions) {
  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  container->SetInsideBorderInsets(gfx::Insets::VH(0, 0));
  container->SetBetweenChildSpacing(kIconAndTextGap);
  container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  container->AddChildView(std::make_unique<views::ImageView>(
      ui::ImageModel::FromVectorIcon(vector_icon, ui::kColorIcon, kIconSize)));

  auto description_column = std::make_unique<views::BoxLayoutView>();
  description_column->SetOrientation(views::BoxLayout::Orientation::kVertical);
  description_column->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  auto* header_view =
      description_column->AddChildView(std::make_unique<views::Label>(
          header_text, views::style::CONTEXT_DIALOG_BODY_TEXT));
  header_view->SetTextStyle(views::style::TextStyle::STYLE_BODY_3_MEDIUM);
  header_view->SetMultiLine(true);
  header_view->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  for (auto description_text : descriptions) {
    auto* description_view =
        description_column->AddChildView(std::make_unique<views::Label>(
            description_text, views::style::CONTEXT_DIALOG_BODY_TEXT));
    description_view->SetMultiLine(true);
    description_view->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
  }

  container->AddChildView(std::move(description_column));
  return container;
}

}  // namespace

AuthenticatorHybridAndSecurityKeySheetView::
    AuthenticatorHybridAndSecurityKeySheetView(
        std::unique_ptr<AuthenticatorHybridAndSecurityKeySheetModel>
            sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)),
      qr_string_(*static_cast<AuthenticatorSheetModelBase*>(model())
                      ->dialog_model()
                      ->cable_qr_string) {}

AuthenticatorHybridAndSecurityKeySheetView::
    ~AuthenticatorHybridAndSecurityKeySheetView() = default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorHybridAndSecurityKeySheetView::BuildStepSpecificContent() {
  auto* sheet_model =
      static_cast<AuthenticatorHybridAndSecurityKeySheetModel*>(model());
  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetBetweenChildSpacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  bool is_create = sheet_model->dialog_model()->request_type ==
                   device::FidoRequestType::kMakeCredential;
  const std::u16string& rp_id =
      base::UTF8ToUTF16(sheet_model->dialog_model()->relying_party_id);
  const std::vector<std::u16string> qr_labels = {l10n_util::GetStringFUTF16(
      is_create ? IDS_WEBAUTHN_USE_YOUR_PHONE_OR_TABLET_CREATE_DESCRIPTION
                : IDS_WEBAUTHN_USE_YOUR_PHONE_OR_TABLET_SIGN_IN_DESCRIPTION,
      rp_id)};
  container->AddChildView(CreateMechanismDescriptionWithIcon(
      kCameraIcon,
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_USE_YOUR_PHONE_OR_TABLET),
      qr_labels));

  container->AddChildView(
      std::make_unique<AuthenticatorQrCenteredView>(qr_string_));

  if (sheet_model->dialog_model()->show_security_key_on_qr_sheet) {
    container->AddChildView(std::make_unique<views::Separator>());

    auto attestation_warning = sheet_model->GetAttestationWarning();
    std::vector<std::u16string> security_key_labels = {
        l10n_util::GetStringFUTF16(
            is_create ? IDS_WEBAUTHN_USE_YOUR_SECURITY_KEY_CREATE_DESCRIPTION
                      : IDS_WEBAUTHN_USE_YOUR_SECURITY_KEY_SIGN_IN_DESCRIPTION,
            rp_id)};
    if (attestation_warning.has_value()) {
      security_key_labels.push_back(attestation_warning.value());
    }
    container->AddChildView(CreateMechanismDescriptionWithIcon(
        kUsbSecurityKeyIcon,
        l10n_util::GetStringUTF16(IDS_WEBAUTHN_USE_YOUR_SECURITY_KEY),
        security_key_labels));
  }

  return std::make_pair(std::move(container), AutoFocus::kNo);
}

BEGIN_METADATA(AuthenticatorHybridAndSecurityKeySheetView)
END_METADATA
