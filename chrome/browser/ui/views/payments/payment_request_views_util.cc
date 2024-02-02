// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/payments/content/icon/icon_size.h"
#include "components/payments/core/payment_options_provider.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "ui/base/default_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace payments {

namespace {

class ThemeTrackingLabel : public views::Label {
  METADATA_HEADER(ThemeTrackingLabel, views::Label)

 public:
  explicit ThemeTrackingLabel(const std::u16string& text) : Label(text) {}
  ~ThemeTrackingLabel() override = default;

  void set_enabled_color_id(ui::ColorId enabled_color_id) {
    enabled_color_id_ = enabled_color_id;
  }

  // views::Label:
  void OnThemeChanged() override {
    Label::OnThemeChanged();
    if (enabled_color_id_.has_value())
      SetEnabledColor(GetColorProvider()->GetColor(*enabled_color_id_));
  }

 private:
  std::optional<ui::ColorId> enabled_color_id_;
};

BEGIN_METADATA(ThemeTrackingLabel)
END_METADATA

class ChromeLogoImageView : public views::ImageView {
  METADATA_HEADER(ChromeLogoImageView, views::ImageView)

 public:
  ChromeLogoImageView() {
    SetCanProcessEventsWithinSubtree(false);
    SetTooltipText(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  }
  ~ChromeLogoImageView() override = default;

  // views::ImageView:
  void OnThemeChanged() override {
    ImageView::OnThemeChanged();
    SetImage(ui::ImageModel::FromResourceId(
        GetNativeTheme()->ShouldUseDarkColors() ? IDR_PRODUCT_LOGO_NAME_22_WHITE
                                                : IDR_PRODUCT_LOGO_NAME_22));
  }
};

BEGIN_METADATA(ChromeLogoImageView)
END_METADATA

// |s1|, |s2|, and |s3| are lines identifying the profile. |s1| is the
// "headline" which may be emphasized depending on |type|. If |enabled| is
// false, the labels will look disabled.
std::unique_ptr<views::View> GetBaseProfileLabel(
    AddressStyleType type,
    const std::u16string& s1,
    const std::u16string& s2,
    const std::u16string& s3,
    std::u16string* accessible_content,
    bool enabled = true) {
  DCHECK(accessible_content);
  std::unique_ptr<views::View> container = std::make_unique<views::View>();
  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  container->SetLayoutManager(std::move(layout));

  if (!s1.empty()) {
    const int text_style =
        type == AddressStyleType::DETAILED
            ? static_cast<int>(views::style::STYLE_EMPHASIZED)
            : static_cast<int>(views::style::STYLE_PRIMARY);
    auto label = std::make_unique<ThemeTrackingLabel>(s1);
    label->SetTextContext(views::style::CONTEXT_LABEL);
    label->SetTextStyle(text_style);
    label->SetID(static_cast<int>(DialogViewID::PROFILE_LABEL_LINE_1));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (!enabled)
      label->set_enabled_color_id(ui::kColorLabelForegroundDisabled);
    container->AddChildView(std::move(label));
  }

  if (!s2.empty()) {
    auto label = std::make_unique<ThemeTrackingLabel>(s2);
    label->SetID(static_cast<int>(DialogViewID::PROFILE_LABEL_LINE_2));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (!enabled)
      label->set_enabled_color_id(ui::kColorLabelForegroundDisabled);
    container->AddChildView(std::move(label));
  }

  if (!s3.empty()) {
    auto label = std::make_unique<ThemeTrackingLabel>(s3);
    label->SetID(static_cast<int>(DialogViewID::PROFILE_LABEL_LINE_3));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (!enabled)
      label->set_enabled_color_id(ui::kColorLabelForegroundDisabled);
    container->AddChildView(std::move(label));
  }

  *accessible_content = l10n_util::GetStringFUTF16(
      IDS_PAYMENTS_PROFILE_LABELS_ACCESSIBLE_FORMAT, s1, s2, s3);

  return container;
}

// Returns a label representing the |profile| as a shipping address. See
// GetBaseProfileLabel() for more documentation.
std::unique_ptr<views::View> GetShippingAddressLabel(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    std::u16string* accessible_content,
    bool enabled) {
  DCHECK(accessible_content);
  std::u16string name = profile.GetInfo(autofill::NAME_FULL, locale);

  std::u16string address =
      GetShippingAddressLabelFromAutofillProfile(profile, locale);

  std::u16string phone =
      autofill::i18n::GetFormattedPhoneNumberForDisplay(profile, locale);

  return GetBaseProfileLabel(type, name, address, phone, accessible_content,
                             enabled);
}

std::unique_ptr<views::Label> GetLabelForMissingInformation(
    const std::u16string& missing_info) {
  auto label = std::make_unique<ThemeTrackingLabel>(missing_info);
  label->SetTextContext(CONTEXT_DIALOG_BODY_TEXT_SMALL);
  label->SetID(static_cast<int>(DialogViewID::PROFILE_LABEL_ERROR));
  // Missing information typically has a nice shade of blue.
  label->set_enabled_color_id(ui::kColorLinkForeground);
  return label;
}

// Paints the gray horizontal line that doesn't span the entire width of the
// dialog at the bottom of the view it borders.
class PaymentRequestRowBorderPainter : public views::Painter {
 public:
  explicit PaymentRequestRowBorderPainter(SkColor color) : color_(color) {}

  PaymentRequestRowBorderPainter(const PaymentRequestRowBorderPainter&) =
      delete;
  PaymentRequestRowBorderPainter& operator=(
      const PaymentRequestRowBorderPainter&) = delete;

  ~PaymentRequestRowBorderPainter() override {}

  // views::Painter:
  gfx::Size GetMinimumSize() const override {
    return gfx::Size(2 * payments::kPaymentRequestRowHorizontalInsets, 1);
  }

  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    int line_height = size.height() - 1;
    canvas->DrawLine(
        gfx::PointF(payments::kPaymentRequestRowHorizontalInsets, line_height),
        gfx::PointF(size.width() - payments::kPaymentRequestRowHorizontalInsets,
                    line_height),
        color_);
  }

 private:
  SkColor color_;
};

}  // namespace

std::unique_ptr<views::ImageView> CreateAppIconView(
    int icon_resource_id,
    const SkBitmap* icon_bitmap,
    const std::u16string& tooltip_text,
    float opacity) {
  std::unique_ptr<views::ImageView> icon_view =
      std::make_unique<views::ImageView>();
  icon_view->SetCanProcessEventsWithinSubtree(false);
  if (icon_bitmap || !icon_resource_id) {
    gfx::ImageSkia img = gfx::ImageSkia::CreateFrom1xBitmap(
                             (icon_bitmap ? *icon_bitmap : SkBitmap()))
                             .DeepCopy();
    icon_view->SetImage(ui::ImageModel::FromImageSkia(img));
    float width = base::checked_cast<float>(img.width());
    float height = base::checked_cast<float>(img.height());
    float ratio = 1;
    if (width && height)
      ratio = width / height;
    // We should set image size in density indepent pixels here, since
    // views::ImageView objects are rastered at the device scale factor.
    icon_view->SetImageSize(gfx::Size(
        ratio * IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight,
        IconSizeCalculator::kPaymentAppDeviceIndependentIdealIconHeight));
  } else {
    icon_view->SetImage(ui::ImageModel::FromResourceId(icon_resource_id));
    // Images from |icon_resource_id| are 32x20 credit cards.
    icon_view->SetImageSize(gfx::Size(32, 20));
  }
  icon_view->SetTooltipText(tooltip_text);
  icon_view->SetPaintToLayer();
  icon_view->layer()->SetFillsBoundsOpaquely(false);
  icon_view->layer()->SetOpacity(opacity);
  return icon_view;
}

std::unique_ptr<views::View> CreateProductLogoFooterView() {
  std::unique_ptr<views::View> content_view = std::make_unique<views::View>();

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  content_view->SetLayoutManager(std::move(layout));

  // Adds the Chrome logo image.
  content_view->AddChildView(std::make_unique<ChromeLogoImageView>());

  return content_view;
}

std::unique_ptr<views::View> GetShippingAddressLabelWithMissingInfo(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    const PaymentsProfileComparator& comp,
    std::u16string* accessible_content,
    bool enabled) {
  DCHECK(accessible_content);
  std::unique_ptr<views::View> base_label = GetShippingAddressLabel(
      type, locale, profile, accessible_content, enabled);

  std::u16string missing = comp.GetStringForMissingShippingFields(profile);
  if (!missing.empty()) {
    base_label->AddChildView(GetLabelForMissingInformation(missing));
    *accessible_content = l10n_util::GetStringFUTF16(
        IDS_PAYMENTS_ACCESSIBLE_LABEL_WITH_ERROR, *accessible_content, missing);
  }

  return base_label;
}

// TODO(anthonyvd): unit test the label layout.
std::unique_ptr<views::View> GetContactInfoLabel(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    bool request_payer_name,
    bool request_payer_email,
    bool request_payer_phone,
    const PaymentsProfileComparator& comp,
    std::u16string* accessible_content) {
  DCHECK(accessible_content);
  std::u16string name = request_payer_name
                            ? profile.GetInfo(autofill::NAME_FULL, locale)
                            : std::u16string();

  std::u16string phone =
      request_payer_phone
          ? autofill::i18n::GetFormattedPhoneNumberForDisplay(profile, locale)
          : std::u16string();

  std::u16string email = request_payer_email
                             ? profile.GetInfo(autofill::EMAIL_ADDRESS, locale)
                             : std::u16string();

  std::unique_ptr<views::View> base_label =
      GetBaseProfileLabel(type, name, phone, email, accessible_content);

  std::u16string missing = comp.GetStringForMissingContactFields(profile);
  if (!missing.empty()) {
    base_label->AddChildView(GetLabelForMissingInformation(missing));
    *accessible_content = l10n_util::GetStringFUTF16(
        IDS_PAYMENTS_ACCESSIBLE_LABEL_WITH_ERROR, *accessible_content, missing);
  }
  return base_label;
}

std::unique_ptr<views::Border> CreatePaymentRequestRowBorder(
    SkColor color,
    const gfx::Insets& insets) {
  return views::CreateBorderPainter(
      std::make_unique<PaymentRequestRowBorderPainter>(color), insets);
}

std::unique_ptr<views::Label> CreateBoldLabel(const std::u16string& text) {
  return std::make_unique<views::Label>(text, views::style::CONTEXT_LABEL,
                                        views::style::STYLE_EMPHASIZED);
}

std::unique_ptr<views::Label> CreateMediumLabel(const std::u16string& text) {
  // TODO(tapted): This should refer to a style in the Chrome typography spec.
  // Also, it needs to handle user setups where the default font is BOLD already
  // since asking for a MEDIUM font will give a lighter font.
  std::unique_ptr<views::Label> label = std::make_unique<views::Label>(text);
  label->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontListForDetails(
          ui::ResourceBundle::FontDetails(std::string(),
                                          ui::kLabelFontSizeDelta,
                                          gfx::Font::Weight::MEDIUM)));
  return label;
}

std::unique_ptr<views::Label> CreateHintLabel(
    const std::u16string& text,
    gfx::HorizontalAlignment alignment) {
  std::unique_ptr<views::Label> label = std::make_unique<views::Label>(
      text, views::style::CONTEXT_LABEL, views::style::STYLE_HINT);
  label->SetHorizontalAlignment(alignment);
  return label;
}

std::unique_ptr<views::View> CreateShippingOptionLabel(
    payments::mojom::PaymentShippingOption* shipping_option,
    const std::u16string& formatted_amount,
    bool emphasize_label,
    std::u16string* accessible_content) {
  DCHECK(accessible_content);
  std::unique_ptr<views::View> container = std::make_unique<views::View>();

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  container->SetLayoutManager(std::move(layout));

  if (shipping_option) {
    const std::u16string& text = base::UTF8ToUTF16(shipping_option->label);
    std::unique_ptr<views::Label> shipping_label =
        emphasize_label ? CreateMediumLabel(text)
                        : std::make_unique<views::Label>(text);
    // Strings from the website may not match the locale of the device, so align
    // them according to the language of the text. This will result, for
    // example, in "he" labels being right-aligned in a browser that's using
    // "en" locale.
    shipping_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
    shipping_label->SetID(
        static_cast<int>(DialogViewID::SHIPPING_OPTION_DESCRIPTION));
    container->AddChildView(std::move(shipping_label));

    std::unique_ptr<views::Label> amount_label =
        std::make_unique<views::Label>(formatted_amount);
    amount_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    amount_label->SetID(static_cast<int>(DialogViewID::SHIPPING_OPTION_AMOUNT));
    container->AddChildView(std::move(amount_label));

    *accessible_content = l10n_util::GetStringFUTF16(
        IDS_PAYMENTS_PROFILE_LABELS_ACCESSIBLE_FORMAT, text, formatted_amount,
        std::u16string());
  }

  return container;
}

std::unique_ptr<views::View> CreateWarningView(const std::u16string& message,
                                               bool show_icon) {
  auto header_view = std::make_unique<views::View>();
  // 8 pixels between the warning icon view (if present) and the text.
  constexpr int kRowHorizontalSpacing = 8;
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, kPaymentRequestRowHorizontalInsets),
      kRowHorizontalSpacing);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  header_view->SetLayoutManager(std::move(layout));

  auto label = std::make_unique<ThemeTrackingLabel>(message);
  // If the warning message comes from the websites, then align label
  // according to the language of the website's text.
  label->SetHorizontalAlignment(message.empty() ? gfx::ALIGN_LEFT
                                                : gfx::ALIGN_TO_HEAD);
  label->SetID(static_cast<int>(DialogViewID::WARNING_LABEL));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (show_icon) {
    auto warning_icon = std::make_unique<views::ImageView>();
    warning_icon->SetCanProcessEventsWithinSubtree(false);
    warning_icon->SetImage(ui::ImageModel::FromVectorIcon(
        vector_icons::kWarningIcon, ui::kColorAlertHighSeverity, 16));
    header_view->AddChildView(std::move(warning_icon));
    label->set_enabled_color_id(ui::kColorAlertHighSeverity);
  }

  header_view->AddChildView(std::move(label));
  return header_view;
}

}  // namespace payments
