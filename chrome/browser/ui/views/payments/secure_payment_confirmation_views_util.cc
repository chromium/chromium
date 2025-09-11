// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/secure_payment_confirmation_views_util.h"

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/theme_resources.h"
#include "components/payments/core/features.h"
#include "components/payments/core/sizes.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
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

class SecurePaymentConfirmationHeaderIconView : public NonAccessibleImageView {
  METADATA_HEADER(SecurePaymentConfirmationHeaderIconView,
                  NonAccessibleImageView)

 public:
  explicit SecurePaymentConfirmationHeaderIconView(bool use_cart_image = false)
      : use_cart_image_{use_cart_image} {
    const gfx::Size header_size(
        GetSecurePaymentConfirmationHeaderWidth(),
        use_cart_image_ ? kShoppingCartHeaderIconHeight : kHeaderIconHeight);
    SetPreferredSize(header_size);
    SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  }
  ~SecurePaymentConfirmationHeaderIconView() override = default;

  // NonAccessibleImageView:
  void OnThemeChanged() override {
    NonAccessibleImageView::OnThemeChanged();
    const bool dark_mode = GetNativeTheme()->preferred_color_scheme() ==
                           ui::NativeTheme::PreferredColorScheme::kDark;
    SetImage(use_cart_image_ ? GetHeaderImageSkia(dark_mode)
                             : ui::ImageModel::FromVectorIcon(
                                   GetPlatformVectorIcon(dark_mode),
                                   gfx::kPlaceholderColor));
  }

 private:
  bool use_cart_image_;
};

BEGIN_METADATA(SecurePaymentConfirmationHeaderIconView)
END_METADATA

}  // namespace

std::unique_ptr<views::View> CreateSecurePaymentConfirmationHeaderIcon(
    int header_icon_id,
    bool use_cart_image) {
  auto image_view =
      std::make_unique<SecurePaymentConfirmationHeaderIconView>(use_cart_image);
  image_view->SetID(header_icon_id);
  image_view->SetProperty(views::kMarginsKey,
                          gfx::Insets().set_top(kHeaderIconTopPadding));
  return image_view;
}

std::unique_ptr<views::Label> CreateSecurePaymentConfirmationTitleLabel(
    const std::u16string& title) {
  std::unique_ptr<views::Label> title_label = std::make_unique<views::Label>(
      title, views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY);
  title_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_label->SetLineHeight(kTitleLineHeight);

  return title_label;
}

std::unique_ptr<views::ImageView> CreateSecurePaymentConfirmationIconView(
    const gfx::ImageSkia& image) {
  std::unique_ptr<views::ImageView> icon_view =
      std::make_unique<views::ImageView>();
  icon_view->SetImage(ui::ImageModel::FromImageSkia(image));

  gfx::Size image_size = image.size();
  // Resize to a constant height, with a variable width in the acceptable range
  // based on the aspect ratio.
  float aspect_ratio =
      static_cast<float>(image_size.width()) / image_size.height();
  int preferred_width =
      static_cast<int>(kSecurePaymentConfirmationIconHeightPx * aspect_ratio);
  int icon_width = std::max(
      std::min(preferred_width, kSecurePaymentConfirmationIconMaximumWidthPx),
      kSecurePaymentConfirmationIconDefaultWidthPx);
  icon_view->SetImageSize(
      gfx::Size(icon_width, kSecurePaymentConfirmationIconHeightPx));
  icon_view->SetPaintToLayer();
  icon_view->layer()->SetFillsBoundsOpaquely(false);

  return icon_view;
}

std::u16string FormatMerchantLabel(
    const std::optional<std::u16string>& merchant_name,
    const std::optional<std::u16string>& merchant_origin) {
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
