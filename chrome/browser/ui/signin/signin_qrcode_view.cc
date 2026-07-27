// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_qrcode_view.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/views/webauthn/authenticator_qr_centered_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_utils.h"

namespace {

constexpr int kBetweenChildSpacing = 24;
constexpr int kQrContainerSize = 120;
constexpr int kLogoBottomSpacing = 10;
constexpr int kLogoSize = 32;
constexpr int kTitleDescriptionSpacing = 4;
constexpr int kQrCodeImageSize = 100;
constexpr int kQrCodeMargin = 20;

}  // namespace

SigninQRCodeView::SigninQRCodeView() {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetBetweenChildSpacing(kBetweenChildSpacing);

  qr_container_ = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .SetPreferredSize(gfx::Size(kQrContainerSize, kQrContainerSize))
          .Build());

  // Add the throbber (spinner) initially.
  qr_container_->AddChildView(views::Builder<views::Throbber>().Build())
      ->Start();

  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .SetBetweenChildSpacing(kLogoBottomSpacing)
          .AddChildren(
              views::Builder<views::ImageView>()
                  .SetImage(ui::ImageModel::FromResourceId(IDR_PRODUCT_LOGO_32))
                  .SetImageSize(gfx::Size(kLogoSize, kLogoSize)),
              views::Builder<views::BoxLayoutView>()
                  .SetOrientation(views::BoxLayout::Orientation::kVertical)
                  .SetCrossAxisAlignment(
                      views::BoxLayout::CrossAxisAlignment::kStart)
                  .SetBetweenChildSpacing(kTitleDescriptionSpacing)
                  .AddChildren(
                      views::Builder<views::Label>()
                          .SetText(l10n_util::GetStringUTF16(
                              IDS_SIGNIN_QRCODE_BANNER_TITLE))
                          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
                          .SetTextStyle(views::style::STYLE_HEADLINE_4)
                          .SetHorizontalAlignment(gfx::ALIGN_LEFT),
                      views::Builder<views::Label>()
                          .SetText(l10n_util::GetStringUTF16(
                              IDS_SIGNIN_QRCODE_BANNER_DESCRIPTION))
                          .SetTextContext(views::style::CONTEXT_LABEL)
                          .SetTextStyle(views::style::STYLE_BODY_3)
                          .SetMultiLine(true)
                          .SetHorizontalAlignment(gfx::ALIGN_LEFT)))
          .Build());
}

SigninQRCodeView::~SigninQRCodeView() = default;

void SigninQRCodeView::UpdateQrCode(std::string_view qr_string) {
  qr_container_->RemoveAllChildViews();

  auto qr_code_view = std::make_unique<AuthenticatorQrCenteredView>(
      std::string(qr_string), kQrCodeImageSize, kQrCodeMargin);
  qr_container_->AddChildView(std::move(qr_code_view));

  // Re-layout the container.
  InvalidateLayout();
}

bool SigninQRCodeView::IsShowingQrCodeForTesting() const {
  if (!qr_container_ || qr_container_->children().empty()) {
    return false;
  }
  return views::IsViewClass<AuthenticatorQrCenteredView>(
      qr_container_->children().front().get());
}

BEGIN_METADATA(SigninQRCodeView)
END_METADATA
