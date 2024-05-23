// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/digital_identity_provider_desktop.h"

#include <memory>

#include "base/containers/span.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/digital_identity_provider.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_handshake.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

namespace {

// Smaller than DistanceMetric::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH.
const int kQrCodeSize = 240;

using RequestStatusForMetrics =
    content::DigitalIdentityProvider::RequestStatusForMetrics;

std::unique_ptr<views::View> MakeQrCodeImageView(const std::string& qr_url) {
  auto qr_code = qr_code_generator::GenerateImage(
      base::as_byte_span(qr_url), qr_code_generator::ModuleStyle::kCircles,
      qr_code_generator::LocatorStyle::kRounded,
      qr_code_generator::CenterImage::kNoCenterImage,
      qr_code_generator::QuietZone::kIncluded);

  // Success is guaranteed, because `qr_url`'s size is bounded and smaller
  // than QR code limits.
  CHECK(qr_code.has_value());
  auto image_view = std::make_unique<views::ImageView>(
      ui::ImageModel::FromImageSkia(qr_code.value()));
  image_view->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_WEB_DIGITAL_CREDENTIALS_QR_CODE_ALT_TEXT));
  image_view->SetImageSize(gfx::Size(kQrCodeSize, kQrCodeSize));
  return std::move(image_view);
}

}  // anonymous namespace

DigitalIdentityProviderDesktop::DigitalIdentityProviderDesktop() = default;
DigitalIdentityProviderDesktop::~DigitalIdentityProviderDesktop() = default;

void DigitalIdentityProviderDesktop::Request(content::WebContents* web_contents,
                                             const url::Origin& rp_origin,
                                             const std::string& request,
                                             DigitalIdentityCallback callback) {
  callback_ = std::move(callback);

  std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key;
  crypto::RandBytes(qr_generator_key);
  std::string qr_url = device::cablev2::qr::Encode(
      qr_generator_key, device::FidoRequestType::kGetAssertion);
  ShowQrCodeDialog(web_contents, rp_origin, qr_url);
}

void DigitalIdentityProviderDesktop::ShowQrCodeDialog(
    content::WebContents* web_contents,
    const url::Origin& rp_origin,
    const std::string& qr_url) {
  std::u16string formatted_rp_origin = l10n_util::GetStringFUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_QR_BODY,
      url_formatter::FormatOriginForSecurityDisplay(
          rp_origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  auto dialog_model =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>())
          .AddCancelButton(base::BindOnce(
              &DigitalIdentityProviderDesktop::OnQrCodeDialogCanceled,
              weak_ptr_factory_.GetWeakPtr()))
          .SetDialogDestroyingCallback(base::BindOnce(
              &DigitalIdentityProviderDesktop::OnQrCodeDialogCanceled,
              weak_ptr_factory_.GetWeakPtr()))
          .SetTitle(
              l10n_util::GetStringUTF16(IDS_WEB_DIGITAL_CREDENTIALS_QR_TITLE))
          .AddParagraph(ui::DialogModelLabel(formatted_rp_origin))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  MakeQrCodeImageView(qr_url),
                  views::BubbleDialogModelHost::FieldType::kText))
          .Build();
  constrained_window::ShowWebModal(std::move(dialog_model), web_contents);
}

void DigitalIdentityProviderDesktop::OnQrCodeDialogCanceled() {
  if (callback_.is_null()) {
    return;
  }

  std::move(callback_).Run(
      base::unexpected(RequestStatusForMetrics::kErrorOther));
}
