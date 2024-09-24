// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_qr_sheet_view.h"

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/webauthn/sheet_models.h"
#include "chrome/grit/generated_resources.h"
#include "components/qr_code_generator/bitmap_generator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"
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

constexpr int kQrCodeMargin = 40;
constexpr int kQrCodeImageSize = 240;
constexpr int kSecurityKeyIconSize = 30;

}  // namespace

class AuthenticatorQRViewCentered : public views::View {
  METADATA_HEADER(AuthenticatorQRViewCentered, views::View)

 public:
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
  gfx::Size qrCodeImageSize() const {
    return gfx::Size(kQrCodeImageSize, kQrCodeImageSize);
  }

  std::string qr_string_;
  raw_ptr<views::ImageView> qr_code_image_;
};

BEGIN_METADATA(AuthenticatorQRViewCentered)
END_METADATA

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
      std::make_unique<AuthenticatorQRViewCentered>(qr_string_));

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
