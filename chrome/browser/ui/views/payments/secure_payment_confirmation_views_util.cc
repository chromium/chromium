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
#include "third_party/blink/public/common/features.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace payments {



std::unique_ptr<views::ImageView> CreateSecurePaymentConfirmationIconView(
    const gfx::ImageSkia& image) {
  return CreateSecurePaymentConfirmationIconView(
      ui::ImageModel::FromImageSkia(image));
}

std::unique_ptr<views::ImageView> CreateSecurePaymentConfirmationIconView(
    const ui::ImageModel& image) {
  std::unique_ptr<views::ImageView> icon_view =
      std::make_unique<views::ImageView>();
  icon_view->SetImage(image);

  gfx::Size image_size = image.Size();
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


}  // namespace payments
