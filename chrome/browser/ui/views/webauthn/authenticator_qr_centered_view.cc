// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_qr_centered_view.h"

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

namespace {

constexpr int kQrCodeMargin = 40;
constexpr int kQrCodeImageSize = 240;

}  // namespace

AuthenticatorQrCenteredView::AuthenticatorQrCenteredView(
    const std::string& qr_string) {
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  qr_code_image_ = AddChildViewAt(std::make_unique<views::ImageView>(), 0);
  qr_code_image_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  qr_code_image_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  qr_code_image_->SetImageSize(qrCodeImageSize());
  qr_code_image_->SetPreferredSize(qrCodeImageSize() +
                                   gfx::Size(kQrCodeMargin, kQrCodeMargin));
  qr_code_image_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_WEBAUTHN_QR_CODE_ALT_TEXT));

  // TODO(https://crbug.com/325664342): Audit if `QuietZone::kIncluded`
  // can/should be used instead (this may require testing if the different
  // image size works well with surrounding UI elements).  Note that the
  // absence of a quiet zone may interfere with decoding of QR codes even for
  // small codes (for examples see #comment8, #comment9 and #comment6 in the
  // bug).
  auto qr_code = qr_code_generator::GenerateImage(
      base::as_byte_span(qr_string), qr_code_generator::ModuleStyle::kCircles,
      qr_code_generator::LocatorStyle::kRounded,
      qr_code_generator::CenterImage::kPasskey,
      qr_code_generator::QuietZone::kWillBeAddedByClient);

  // Success is guaranteed, because `qr_string`'s size is bounded and smaller
  // than QR code limits.
  CHECK(qr_code.has_value(), base::NotFatalUntil::M124);

  qr_code_image_->SetImage(ui::ImageModel::FromImageSkia(qr_code.value()));
  qr_code_image_->SetVisible(true);
}

AuthenticatorQrCenteredView::~AuthenticatorQrCenteredView() = default;

void AuthenticatorQrCenteredView::OnThemeChanged() {
  views::View::OnThemeChanged();

  const int border_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kHigh);
  qr_code_image_->SetBackground(views::CreateRoundedRectBackground(
      GetColorProvider()->GetColor(kColorQrCodeBackground), border_radius, 2));
}

gfx::Size AuthenticatorQrCenteredView::qrCodeImageSize() const {
  return gfx::Size(kQrCodeImageSize, kQrCodeImageSize);
}

BEGIN_METADATA(AuthenticatorQrCenteredView)
END_METADATA
