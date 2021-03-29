// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"

#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace payments {
namespace {

const gfx::VectorIcon& GetPlatformVectorIcon(bool dark_mode) {
#if defined(OS_WIN)
  return dark_mode ? kSecurePaymentConfirmationFaceDarkIcon
                   : kSecurePaymentConfirmationFaceIcon;
#else
  return dark_mode ? kSecurePaymentConfirmationFingerprintDarkIcon
                   : kSecurePaymentConfirmationFingerprintIcon;
#endif
}

}  // namespace

int GetSecurePaymentConfirmationHeaderWidth() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
}

std::unique_ptr<views::View> CreateSecurePaymentConfirmationIconView(
    bool dark_mode) {
  const int header_width = GetSecurePaymentConfirmationHeaderWidth();
  const gfx::Size header_size(header_width, kHeaderIconHeight);

  auto image_view = std::make_unique<NonAccessibleImageView>();
  image_view->SetImage(gfx::CreateVectorIcon(GetPlatformVectorIcon(dark_mode)));
  image_view->SetSize(header_size);
  image_view->SetPreferredSize(header_size);
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kLeading);

  return image_view;
}

std::unique_ptr<views::ProgressBar>
CreateSecurePaymentConfirmationProgressBarView() {
  auto progress_bar = std::make_unique<views::ProgressBar>(
      kProgressBarHeight, /*allow_round_corner=*/false);
  progress_bar->SetValue(-1);  // infinite animation.
  progress_bar->SetBackgroundColor(SK_ColorTRANSPARENT);
  progress_bar->SetPreferredSize(
      gfx::Size(GetSecurePaymentConfirmationHeaderWidth(), kProgressBarHeight));
  progress_bar->SizeToPreferredSize();

  return progress_bar;
}

std::unique_ptr<views::View> CreateSecurePaymentConfirmationHeaderView(
    bool dark_mode,
    int progress_bar_id,
    int header_icon_id) {
  auto header = std::make_unique<views::View>();

  views::GridLayout* layout =
      header->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 1.0,
                     views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  // Progress bar
  layout->StartRow(views::GridLayout::kFixedSize, 0, kProgressBarHeight);
  auto progress_bar = CreateSecurePaymentConfirmationProgressBarView();
  progress_bar->SetID(progress_bar_id);
  progress_bar->SetVisible(false);
  layout->AddView(std::move(progress_bar));

  layout->AddPaddingRow(views::GridLayout::kFixedSize, kHeaderIconTopPadding);

  // Header icon
  layout->StartRow(views::GridLayout::kFixedSize, 0, kHeaderIconHeight);
  auto image_view = CreateSecurePaymentConfirmationIconView(dark_mode);
  image_view->SetID(header_icon_id);
  layout->AddView(std::move(image_view));

  return header;
}

std::unique_ptr<views::Label> CreateSecurePaymentConfirmationTitleLabel(
    const std::u16string& title) {
  std::unique_ptr<views::Label> title_label = std::make_unique<views::Label>(
      title, views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY);
  title_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_label->SetLineHeight(kTitleLineHeight);
  title_label->SetBorder(views::CreateEmptyBorder(0, 0, kBodyInsets, 0));

  return title_label;
}

std::unique_ptr<views::ImageView>
CreateSecurePaymentConfirmationInstrumentIconView(const SkBitmap& bitmap) {
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap).DeepCopy();

  std::unique_ptr<views::ImageView> icon_view =
      std::make_unique<views::ImageView>();
  icon_view->SetImage(image);
  icon_view->SetImageSize(
      gfx::Size(kInstrumentIconWidth, kInstrumentIconHeight));
  icon_view->SetPaintToLayer();
  icon_view->layer()->SetFillsBoundsOpaquely(false);

  return icon_view;
}

}  // namespace payments
