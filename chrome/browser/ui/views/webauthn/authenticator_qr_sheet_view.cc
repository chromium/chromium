// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_qr_sheet_view.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/services/qrcode_generator/public/cpp/qrcode_generator_service.h"
#include "chrome/services/qrcode_generator/public/mojom/qrcode_generator.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

namespace {

constexpr int kQrCodeMargin = 40;
constexpr int kQrCodeImageSize = 240;

}  // namespace

class AuthenticatorQRViewCentered : public views::View {
 public:
  METADATA_HEADER(AuthenticatorQRViewCentered);
  explicit AuthenticatorQRViewCentered(const std::string& qr_string) {
    views::BoxLayout* layout =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    qr_code_image_ = AddChildViewAt(std::make_unique<views::ImageView>(), 0);
    qr_code_image_->SetHorizontalAlignment(
        views::ImageView::Alignment::kCenter);
    qr_code_image_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
    qr_code_image_->SetImageSize(qrCodeImageSize());
    qr_code_image_->SetPreferredSize(qrCodeImageSize() +
                                     gfx::Size(kQrCodeMargin, kQrCodeMargin));

    qrcode_generator::mojom::GenerateQRCodeRequestPtr request =
        qrcode_generator::mojom::GenerateQRCodeRequest::New();
    request->data = qr_string;
    request->center_image = qrcode_generator::mojom::CenterImage::PASSKEY_ICON;

    request->render_module_style =
        qrcode_generator::mojom::ModuleStyle::CIRCLES;
    request->render_locator_style =
        qrcode_generator::mojom::LocatorStyle::ROUNDED;

    // Deleting the view will close the channel so base::Unretained is safe
    // here.
    auto callback =
        base::BindOnce(&AuthenticatorQRViewCentered::OnQrCodeGenerated,
                       base::Unretained(this));
    qr_code_service().GenerateQRCode(std::move(request), std::move(callback));
  }

  ~AuthenticatorQRViewCentered() override = default;

  AuthenticatorQRViewCentered(const AuthenticatorQRViewCentered&) = delete;
  AuthenticatorQRViewCentered& operator=(const AuthenticatorQRViewCentered&) =
      delete;

  void OnThemeChanged() override {
    views::View::OnThemeChanged();

    const int border_radius =
        views::LayoutProvider::Get()->GetCornerRadiusMetric(
            views::Emphasis::kHigh);
    qr_code_image_->SetBackground(views::CreateRoundedRectBackground(
        GetColorProvider()->GetColor(kColorQrCodeBackground), border_radius,
        2));
  }

 private:
  qrcode_generator::QRImageGenerator& qr_code_service() {
    if (!qr_code_service_) {
      qr_code_service_ = std::make_unique<qrcode_generator::QRImageGenerator>();
    }
    return *qr_code_service_;
  }

  gfx::Size qrCodeImageSize() const {
    return gfx::Size(kQrCodeImageSize, kQrCodeImageSize);
  }

  void OnQrCodeGenerated(
      const qrcode_generator::mojom::GenerateQRCodeResponsePtr response) {
    DCHECK(response->error_code ==
           qrcode_generator::mojom::QRCodeGeneratorError::NONE);
    qr_code_image_->SetImage(
        gfx::ImageSkia::CreateFrom1xBitmap(response->bitmap));
    qr_code_image_->SetVisible(true);
  }

  std::string qr_string_;
  raw_ptr<views::ImageView> qr_code_image_;

  // TODO(https://crbug.com/1431991): Remove this field once there is no
  // internal state (e.g. no `mojo::Remote`) that needs to be maintained by the
  // `QRImageGenerator` class.
  std::unique_ptr<qrcode_generator::QRImageGenerator> qr_code_service_;
};

BEGIN_METADATA(AuthenticatorQRViewCentered, views::View)
END_METADATA

AuthenticatorQRSheetView::AuthenticatorQRSheetView(
    std::unique_ptr<AuthenticatorQRSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)),
      qr_string_(static_cast<AuthenticatorQRSheetModel*>(model())
                     ->dialog_model()
                     ->cable_qr_string()) {}

AuthenticatorQRSheetView::~AuthenticatorQRSheetView() = default;

std::pair<std::unique_ptr<views::View>,
          AuthenticatorRequestSheetView::AutoFocus>
AuthenticatorQRSheetView::BuildStepSpecificContent() {
  auto qr_view = std::make_unique<AuthenticatorQRViewCentered>(qr_string_);
  qr_view_ = qr_view.get();

  return std::make_pair(std::move(qr_view), AutoFocus::kYes);
}
