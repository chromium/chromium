// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/theme_tracking_image_view.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

// Width of the Google Pay logo if used, as it is not square.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr int kGooglePayLogoWidth = 40;
#endif
constexpr int kIconHeight = 16;
// The size of the icons in the view created by `CreateTextWithIconView()`.
constexpr int kTextWithIconViewIconSize = 24;

std::unique_ptr<views::ImageView> CreateIconView(
    TitleWithIconAfterLabelView::Icon icon_to_show,
    const base::RepeatingCallback<ui::ColorVariant()>&
        get_background_color_callback) {
  ui::ImageModel model;
  std::optional<ui::ImageModel> model_dark;
  switch (icon_to_show) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case TitleWithIconAfterLabelView::Icon::GOOGLE_PAY:
      model = ui::ImageModel::FromImageGenerator(
          base::BindRepeating(&CreateTiledGooglePayLogo, kGooglePayLogoWidth,
                              kIconHeight),
          gfx::Size(kGooglePayLogoWidth, kIconHeight));
      break;
    case TitleWithIconAfterLabelView::Icon::GOOGLE_PAY_AND_AFFIRM:
      model = ui::ImageModel::FromImageSkia(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_AUTOFILL_GOOGLE_PAY_AFFIRM));
      model_dark = ui::ImageModel::FromImageSkia(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_AUTOFILL_GOOGLE_PAY_AFFIRM_DARK));
      break;
    case TitleWithIconAfterLabelView::Icon::GOOGLE_PAY_AND_AFTERPAY:
      model = ui::ImageModel::FromImageSkia(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_AUTOFILL_GOOGLE_PAY_AFTERPAY));
      model_dark = ui::ImageModel::FromImageSkia(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_AUTOFILL_GOOGLE_PAY_AFTERPAY_DARK));
      break;
    case TitleWithIconAfterLabelView::Icon::GOOGLE_PAY_AND_ZIP:
      model = ui::ImageModel::FromImageSkia(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_AUTOFILL_GOOGLE_PAY_ZIP));
      model_dark = ui::ImageModel::FromImageSkia(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_AUTOFILL_GOOGLE_PAY_ZIP_DARK));
      break;
    case TitleWithIconAfterLabelView::Icon::GOOGLE_PAY_AND_KLARNA:
      model = ui::ImageModel::FromImageSkia(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_AUTOFILL_GOOGLE_PAY_KLARNA));
      model_dark = ui::ImageModel::FromImageSkia(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_AUTOFILL_GOOGLE_PAY_KLARNA_DARK));
      break;
    case TitleWithIconAfterLabelView::Icon::GOOGLE_G: {
      const gfx::VectorIcon& icon = vector_icons::kGoogleGLogoIcon;
#else
    case TitleWithIconAfterLabelView::Icon::GOOGLE_PAY:
    case TitleWithIconAfterLabelView::Icon::GOOGLE_G:
    case TitleWithIconAfterLabelView::Icon::GOOGLE_PAY_AND_AFFIRM:
    case TitleWithIconAfterLabelView::Icon::GOOGLE_PAY_AND_AFTERPAY:
    case TitleWithIconAfterLabelView::Icon::GOOGLE_PAY_AND_ZIP:
    case TitleWithIconAfterLabelView::Icon::GOOGLE_PAY_AND_KLARNA: {
      const gfx::VectorIcon& icon = kCreditCardIcon;
#endif
      model = ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, kIconHeight);
      break;
    }
  }
  return std::make_unique<views::ThemeTrackingImageView>(
      model, model_dark.value_or(model), get_background_color_callback);
}

}  // namespace

TextLinkInfo::TextLinkInfo() = default;

TextLinkInfo::TextLinkInfo(const TextLinkInfo& other) = default;
TextLinkInfo& TextLinkInfo::operator=(const TextLinkInfo& other) = default;
TextLinkInfo::TextLinkInfo(TextLinkInfo&& other) = default;
TextLinkInfo& TextLinkInfo::operator=(TextLinkInfo&& other) = default;

TextLinkInfo::~TextLinkInfo() = default;

LabeledTextfieldWithErrorMessage::LabeledTextfieldWithErrorMessage() = default;

LabeledTextfieldWithErrorMessage::LabeledTextfieldWithErrorMessage(
    LabeledTextfieldWithErrorMessage&& other) = default;
LabeledTextfieldWithErrorMessage& LabeledTextfieldWithErrorMessage::operator=(
    LabeledTextfieldWithErrorMessage&& other) = default;

LabeledTextfieldWithErrorMessage::~LabeledTextfieldWithErrorMessage() = default;

views::Textfield& LabeledTextfieldWithErrorMessage::GetInputTextField() const {
  CHECK(input);
  return *input;
}

void LabeledTextfieldWithErrorMessage::SetErrorState(bool is_valid) {
  CHECK(input);
  is_valid_input = is_valid;
  input->SetInvalid(!is_valid);
  if (error_label) {
    error_label->SetVisible(!is_valid);
  }
  if (error_label_placeholder) {
    error_label_placeholder->SetVisible(is_valid);
  }
}

void LabeledTextfieldWithErrorMessage::MaybeAnnounceError() {
  if (!GetInputTextField().GetText().empty() && !is_valid_input) {
    error_label->GetViewAccessibility().AnnouncePolitely(
        error_label->GetText());
  }
}

ui::ImageModel GetProfileAvatar(const AccountInfo& account_info) {
  // Get the user avatar icon.
  gfx::Image account_avatar = account_info.account_image;

  // Check if the avatar is empty, and if so, replace it with a placeholder.
  if (account_avatar.IsEmpty()) {
    account_avatar = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        profiles::GetPlaceholderAvatarIconResourceID());
  }

  int avatar_size = views::TypographyProvider::Get().GetLineHeight(
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);

  return ui::ImageModel::FromImage(profiles::GetSizedAvatarIcon(
      account_avatar, avatar_size, avatar_size, profiles::SHAPE_CIRCLE));
}

TitleWithIconAfterLabelView::TitleWithIconAfterLabelView(
    const std::u16string& window_title,
    TitleWithIconAfterLabelView::Icon icon_to_show) {
  SetBetweenChildSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  // Align to the top instead of center in vertical direction so that we
  // can adjust the icon location to align with the first line of title label
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);

  auto* title_label = AddChildView(std::make_unique<views::Label>(
      window_title, views::style::CONTEXT_DIALOG_TITLE));
  title_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  title_label->SetMultiLine(true);
  auto* icon_view = AddChildView(CreateIconView(
      icon_to_show,
      base::BindRepeating(
          [](views::View* view) {
            return ui::ColorVariant(
                view->GetColorProvider()->GetColor(ui::kColorDialogBackground));
          },
          base::Unretained(this))));

  // Center the icon against the first line of the title label. This needs to be
  // done after we create the title label, so that we can use its preferred
  // size. Depending on which height is larger, the inset will be applied to the
  // view with the smaller height.
  const int title_label_height =
      title_label->GetPreferredSize(views::SizeBounds(title_label->width(), {}))
          .height();
  const int icon_image_height = icon_view->GetImageModel().Size().height();
  if (title_label_height > icon_image_height) {
    icon_view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets().set_top((title_label_height - icon_image_height) / 2)));
  } else {
    title_label->SetBorder(views::CreateEmptyBorder(
        gfx::Insets().set_top((icon_image_height - title_label_height) / 2)));
  }

  // Flex |title_label| to fill up remaining space and tail align the GPay icon.
  SetFlexForView(title_label, 1);
}

TitleWithIconAfterLabelView::~TitleWithIconAfterLabelView() = default;

// TODO(crbug.com/40914021): Replace GetMinimumSize() may generate views
// narrower than expected. The ideal solution should be limit the width of
// multi-line text views.
gfx::Size TitleWithIconAfterLabelView::GetMinimumSize() const {
  // Default View::GetMinimumSize() will make dialogs wider than it should.
  // To avoid that, just return 0x0.
  return gfx::Size(0, 0);
}

BEGIN_METADATA(TitleWithIconAfterLabelView)
END_METADATA

PaymentsUiClosedReason GetPaymentsUiClosedReasonFromWidget(
    const views::Widget* widget) {
  DCHECK(widget);
  if (!widget->IsClosed()) {
    return PaymentsUiClosedReason::kUnknown;
  }

  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kUnspecified:
      return PaymentsUiClosedReason::kNotInteracted;
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
      return PaymentsUiClosedReason::kClosed;
    case views::Widget::ClosedReason::kLostFocus:
      return PaymentsUiClosedReason::kLostFocus;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      return PaymentsUiClosedReason::kAccepted;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      return PaymentsUiClosedReason::kCancelled;
  }
}

std::unique_ptr<views::View> CreateLegalMessageView(
    const LegalMessageLines& legal_message_lines,
    const std::u16string& user_email,
    const ui::ImageModel& user_avatar,
    base::RepeatingCallback<void(const GURL&)> callback) {
  auto result = views::Builder<views::BoxLayoutView>()
                    .SetOrientation(views::BoxLayout::Orientation::kVertical)
                    .SetBetweenChildSpacing(
                        ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_RELATED_CONTROL_VERTICAL_SMALL))
                    .Build();
  for (const LegalMessageLine& line : legal_message_lines) {
    auto label = views::Builder<views::StyledLabel>()
                     .SetText(line.text())
                     .SetTextContext(CONTEXT_DIALOG_BODY_TEXT_SMALL)
                     .SetDefaultTextStyle(views::style::STYLE_SECONDARY)
                     .Build();
    for (const LegalMessageLine::Link& link : line.links()) {
      label->AddStyleRange(link.range,
                           views::StyledLabel::RangeStyleInfo::CreateForLink(
                               base::BindRepeating(callback, link.url)));
    }
    result->AddChildView(std::move(label));
  }

  if (user_email.empty() || user_avatar.IsEmpty()) {
    return result;
  }

  // Extra child view for user identity information including the avatar and
  // the email.
  result->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBetweenChildSpacing(
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL))
          .SetID(DialogViewId::USER_INFORMATION_VIEW)
          .AddChildren(views::Builder<views::ImageView>().SetImage(user_avatar),
                       views::Builder<views::Label>()
                           .SetText(user_email)
                           .SetTextContext(CONTEXT_DIALOG_BODY_TEXT_SMALL)
                           .SetTextStyle(views::style::STYLE_SECONDARY))
          .Build());
  return result;
}

std::unique_ptr<views::View> CreateProgressBarWithTextView(
    const std::u16string& progress_bar_text) {
  views::Throbber* throbber = nullptr;
  auto result =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  views::DISTANCE_RELATED_CONTROL_VERTICAL))
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter)
          .AddChildren(
              views::Builder<views::Throbber>().CopyAddressTo(&throbber),
              views::Builder<views::Label>()
                  .SetText(progress_bar_text)
                  .SetEnabledColor(ui::kColorThrobber))
          .Build();
  throbber->Start();
  return result;
}

std::unique_ptr<views::View> CreateTextWithIconView(
    const std::u16string& text,
    std::optional<TextLinkInfo> text_link_info,
    const gfx::VectorIcon& icon) {
  ChromeLayoutProvider* chrome_layout_provider = ChromeLayoutProvider::Get();

  views::Builder<views::BoxLayoutView> view_builder =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBetweenChildSpacing(chrome_layout_provider->GetDistanceMetric(
              views::DISTANCE_RELATED_LABEL_HORIZONTAL))
          .AddChild(
              views::Builder<views::ImageView>()
                  .SetVerticalAlignment(views::ImageView::Alignment::kLeading)
                  .SetBorder(views::CreateEmptyBorder(
                      chrome_layout_provider->GetInsetsMetric(
                          views::INSETS_LABEL_BUTTON)))
                  .SetImage(ui::ImageModel::FromVectorIcon(
                      icon, ui::kColorIcon, kTextWithIconViewIconSize)));

  if (text_link_info.has_value()) {
    view_builder.AddChild(
        views::Builder<views::StyledLabel>()
            .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
            .SetDefaultTextStyle(views::style::STYLE_SECONDARY)
            .SetText(text)
            .AddStyleRange(text_link_info->offset,
                           views::StyledLabel::RangeStyleInfo::CreateForLink(
                               text_link_info->callback)));
  } else {
    view_builder.AddChild(
        views::Builder<views::Label>()
            .SetMultiLine(true)
            .SetHorizontalAlignment(gfx::ALIGN_TO_HEAD)
            .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
            .SetTextStyle(views::style::STYLE_SECONDARY)
            .SetText(text));
  }

  return std::move(view_builder).Build();
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// kGooglePayLogoIcon is square overall, despite the drawn portion being a
// rectangular area at the top. CreateTiledImage() will correctly clip it
// whereas setting the icon size would rescale it incorrectly and keep the
// bottom empty portion.
gfx::ImageSkia CreateTiledGooglePayLogo(int width,
                                        int height,
                                        const ui::ColorProvider* provider) {
  return gfx::ImageSkiaOperations::CreateTiledImage(
      gfx::CreateVectorIcon(vector_icons::kGooglePayLogoIcon,
                            provider->GetColor(kColorPaymentsGooglePayLogo)),
      /*x=*/0, /*y=*/0, width, height);
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

LabeledTextfieldWithErrorMessage CreateLabelAndTextfieldView(
    const std::u16string& label_text,
    std::optional<std::u16string> error_message) {
  LabeledTextfieldWithErrorMessage result;

  result.container =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build();
  result.container->AddChildView(
      views::Builder<views::Label>()
          .SetText(label_text)
          .SetTextContext(views::style::CONTEXT_LABEL)
          .SetTextStyle(views::style::STYLE_PRIMARY)
          .SetHorizontalAlignment(gfx::ALIGN_TO_HEAD)
          .Build());
  result.container->AddChildView(
      views::Builder<views::View>()
          .SetPreferredSize(
              gfx::Size(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                               views::DISTANCE_RELATED_CONTROL_VERTICAL)))
          .Build());
  result.input = result.container->AddChildView(
      views::Builder<views::Textfield>().SetAccessibleName(label_text).Build());

  if (error_message.has_value()) {
    result.error_label = result.container->AddChildView(
        views::Builder<views::Label>()
            .SetText(*error_message)
            .SetTextContext(views::style::CONTEXT_TEXTFIELD_SUPPORTING_TEXT)
            .SetTextStyle(STYLE_RED)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
            .SetVisible(false)
            .SetEnabled(false)
            .SetMultiLine(true)
            .Build());
    // Add a padding view which will be visible initially to reserve
    // space for the error label. This helps maintain the height of the
    // dialog regardless of whether the error message is visible or not,
    // preventing the dialog from shifting and stretching. When the error
    // message appears, this padding is hidden, and when the error message
    // disappears, the padding reappears to fill the space.
    result.error_label_placeholder = result.container->AddChildView(
        views::Builder<views::View>()
            .SetPreferredSize(result.error_label->GetPreferredSize(
                views::SizeBounds(result.error_label->width(), {})))
            .SetVisible(true)
            .Build());
  }
  return result;
}

}  // namespace autofill
