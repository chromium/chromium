// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"

#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/theme_resources.h"
#include "components/payments/core/sizes.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace payments {
namespace {

const gfx::VectorIcon& GetPlatformVectorIcon(bool dark_mode) {
#if BUILDFLAG(IS_WIN)
  return dark_mode ? kSecurePaymentConfirmationFaceDarkIcon
                   : kSecurePaymentConfirmationFaceIcon;
#else
  return dark_mode ? kSecurePaymentConfirmationFingerprintDarkIcon
                   : kSecurePaymentConfirmationFingerprintIcon;
#endif
}

int GetSecurePaymentConfirmationHeaderWidth() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
}

ui::ImageModel GetHeaderImageSkia(bool dark_mode) {
  return ui::ImageModel::FromResourceId(dark_mode ? IDR_SAVE_CARD_DARK
                                                  : IDR_SAVE_CARD);
}

class SecurePaymentConfirmationIconView : public NonAccessibleImageView {
 public:
  METADATA_HEADER(SecurePaymentConfirmationIconView);

  explicit SecurePaymentConfirmationIconView(bool use_cart_image = false)
      : use_cart_image_{use_cart_image} {
    const gfx::Size header_size(
        GetSecurePaymentConfirmationHeaderWidth(),
        use_cart_image_ ? kShoppingCartHeaderIconHeight : kHeaderIconHeight);
    SetSize(header_size);
    SetPreferredSize(header_size);
    SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  }
  ~SecurePaymentConfirmationIconView() override = default;

  // NonAccessibleImageView:
  void OnThemeChanged() override {
    NonAccessibleImageView::OnThemeChanged();
    SetImage(use_cart_image_
                 ? GetHeaderImageSkia(GetNativeTheme()->ShouldUseDarkColors())
                 : ui::ImageModel::FromVectorIcon(
                       GetPlatformVectorIcon(
                           GetNativeTheme()->ShouldUseDarkColors()),
                       gfx::kPlaceholderColor));
  }

 private:
  bool use_cart_image_;
};

BEGIN_METADATA(SecurePaymentConfirmationIconView, NonAccessibleImageView)
END_METADATA

}  // namespace

std::unique_ptr<views::ProgressBar>
CreateSecurePaymentConfirmationProgressBarView() {
  auto progress_bar = std::make_unique<views::ProgressBar>();
  progress_bar->SetPreferredHeight(kProgressBarHeight);
  progress_bar->SetPreferredCornerRadii(absl::nullopt);
  progress_bar->SetValue(-1);  // infinite animation.
  progress_bar->SetBackgroundColor(SK_ColorTRANSPARENT);
  progress_bar->SetPreferredSize(
      gfx::Size(GetSecurePaymentConfirmationHeaderWidth(), kProgressBarHeight));
  progress_bar->SizeToPreferredSize();

  return progress_bar;
}

std::unique_ptr<views::View> CreateSecurePaymentConfirmationHeaderView(
    int progress_bar_id,
    int header_icon_id,
    bool use_cart_image) {
  auto header = std::make_unique<views::BoxLayoutView>();
  header->SetOrientation(views::BoxLayout::Orientation::kVertical);
  header->SetBetweenChildSpacing(kHeaderIconTopPadding);

  // Progress bar
  auto progress_bar = CreateSecurePaymentConfirmationProgressBarView();
  progress_bar->SetID(progress_bar_id);
  progress_bar->SetVisible(false);
  auto* container = header->AddChildView(std::make_unique<views::View>());
  container->SetPreferredSize(progress_bar->GetPreferredSize());
  container->AddChildView(std::move(progress_bar));

  // Header icon
  auto image_view =
      std::make_unique<SecurePaymentConfirmationIconView>(use_cart_image);
  image_view->SetID(header_icon_id);
  header->AddChildView(std::move(image_view));

  return header;
}

std::unique_ptr<views::Label> CreateSecurePaymentConfirmationTitleLabel(
    const std::u16string& title) {
  std::unique_ptr<views::Label> title_label = std::make_unique<views::Label>(
      title, views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY);
  title_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_label->SetLineHeight(kTitleLineHeight);
  title_label->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(0, 0, kBodyInsets, 0)));

  return title_label;
}

std::unique_ptr<views::ImageView>
CreateSecurePaymentConfirmationInstrumentIconView(const gfx::ImageSkia& image) {
  std::unique_ptr<views::ImageView> icon_view =
      std::make_unique<views::ImageView>();
  icon_view->SetImage(ui::ImageModel::FromImageSkia(image));

  gfx::Size image_size = image.size();
  // Resize to a constant height, with a variable width in the acceptable range
  // based on the aspect ratio.
  float aspect_ratio =
      static_cast<float>(image_size.width()) / image_size.height();
  int preferred_width = static_cast<int>(
      kSecurePaymentConfirmationInstrumentIconHeightPx * aspect_ratio);
  int icon_width =
      std::max(std::min(preferred_width,
                        kSecurePaymentConfirmationInstrumentIconMaximumWidthPx),
               kSecurePaymentConfirmationInstrumentIconDefaultWidthPx);
  icon_view->SetImageSize(
      gfx::Size(icon_width, kSecurePaymentConfirmationInstrumentIconHeightPx));
  icon_view->SetPaintToLayer();
  icon_view->layer()->SetFillsBoundsOpaquely(false);

  return icon_view;
}

std::u16string FormatMerchantLabel(
    const absl::optional<std::u16string>& merchant_name,
    const absl::optional<std::u16string>& merchant_origin) {
  DCHECK(merchant_name.has_value() || merchant_origin.has_value());

  if (merchant_name.has_value() && merchant_origin.has_value()) {
    return base::StrCat(
        {merchant_name.value(), u" (", merchant_origin.value(), u")"});
  }
  return merchant_name.value_or(merchant_origin.value_or(u""));
}

std::unique_ptr<views::StyledLabel> CreateSecurePaymentConfirmationOptOutView(
    const std::u16string& relying_party_id,
    const std::u16string& opt_out_label,
    const std::u16string& opt_out_link_label,
    base::RepeatingClosure on_click) {
  // The opt-out text consists of a base label, filled in with the relying party
  // ID and a 'call to action' link.
  std::vector<std::u16string> subst{relying_party_id, opt_out_link_label};
  std::vector<size_t> offsets;
  std::u16string opt_out_text =
      base::ReplaceStringPlaceholders(opt_out_label, subst, &offsets);
  DCHECK_EQ(2U, offsets.size());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(on_click);

  return views::Builder<views::StyledLabel>()
      .SetText(opt_out_text)
      .SetTextContext(ChromeTextContext::CONTEXT_DIALOG_BODY_TEXT_SMALL)
      .SetDefaultTextStyle(views::style::STYLE_SECONDARY)
      .AddStyleRange(
          gfx::Range(offsets[1], offsets[1] + opt_out_link_label.length()),
          link_style)
      .SetProperty(views::kMarginsKey,
                   gfx::Insets::TLBR(
                       kSecondarySmallTextInsets, kSecondarySmallTextInsets,
                       kSecondarySmallTextInsets, kSecondarySmallTextInsets))
      .SizeToFit(views::LayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH))
      .Build();
}

}  // namespace payments
