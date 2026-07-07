// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_qrcode_infobar.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/signin/signin_qrcode_infobar_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

namespace {

// Visual constants.
constexpr int kBetweenChildSpacing = 24;
constexpr int kHorizontalMargin = 24;
constexpr int kVerticalMargin = 8;
constexpr int kTargetHeight = 133;
constexpr int kQrContainerWidth = 113;
constexpr int kQrContainerHeight = 117;
constexpr int kTextSpacing = 10;
constexpr int kLogoSize = 32;

}  // namespace

SigninQRCodeInfoBar::SigninQRCodeInfoBar(
    std::unique_ptr<SigninQRCodeInfoBarDelegate> delegate)
    : InfoBarView(std::move(delegate)) {
  // Override InfoBarView default layout manager.
  auto* layout =
      content_container()->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::TLBR(kVerticalMargin, kHorizontalMargin, kVerticalMargin,
                            kHorizontalMargin),
          kBetweenChildSpacing));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  SetTargetHeight(kTargetHeight);

  // Create QR Container.
  qr_container_ = content_container()->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .SetPreferredSize(gfx::Size(kQrContainerWidth, kQrContainerHeight))
          .Build());

  content_container()->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .SetBetweenChildSpacing(kTextSpacing)
          .AddChildren(
              views::Builder<views::ImageView>()
                  .SetImage(ui::ImageModel::FromResourceId(IDR_PRODUCT_LOGO_32))
                  .SetImageSize(gfx::Size(kLogoSize, kLogoSize)),
              views::Builder<views::Label>()
                  .SetText(
                      l10n_util::GetStringUTF16(IDS_SIGNIN_QRCODE_BANNER_TITLE))
                  .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
                  .SetTextStyle(views::style::STYLE_HEADLINE_2),
              views::Builder<views::Label>()
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_SIGNIN_QRCODE_BANNER_DESCRIPTION))
                  .SetTextContext(views::style::CONTEXT_LABEL)
                  .SetTextStyle(views::style::STYLE_BODY_1)
                  .SetMultiLine(true))
          .Build());
}

SigninQRCodeInfoBar::~SigninQRCodeInfoBar() = default;

void SigninQRCodeInfoBar::PlatformSpecificShow(bool animate) {
  InfoBarView::PlatformSpecificShow(animate);
  if (parent()) {
    views::View* shadow = parent()->children().back().get();
    if (shadow) {
      shadow->SetVisible(false);
    }
  }
}

BEGIN_METADATA(SigninQRCodeInfoBar)
END_METADATA
