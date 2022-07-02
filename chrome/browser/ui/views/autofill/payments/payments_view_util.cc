// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"

#include "base/bind.h"
#include "base/ranges/algorithm.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace autofill {

namespace {

// Width of the Google Pay logo if used, as it is not square.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr int kGooglePayLogoWidth = 40;
#endif
constexpr int kIconHeight = 16;

constexpr int kSeparatorHeight = 12;

class IconView : public views::ImageView {
 public:
  METADATA_HEADER(IconView);

  explicit IconView(TitleWithIconAndSeparatorView::Icon icon_to_show) {
    icon_to_show_ = icon_to_show;
  }

  // views::ImageView:
  void OnThemeChanged() override {
    ImageView::OnThemeChanged();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    gfx::ImageSkia image;
    switch (icon_to_show_) {
      case TitleWithIconAndSeparatorView::Icon::GOOGLE_PAY:
        // kGooglePayLogoIcon is square overall, despite the drawn portion being
        // a rectangular area at the top. CreateTiledImage() will correctly clip
        // it whereas setting the icon size would rescale it incorrectly and
        // keep the bottom empty portion.
        image = gfx::ImageSkiaOperations::CreateTiledImage(
            gfx::CreateVectorIcon(
                kGooglePayLogoIcon,
                GetColorProvider()->GetColor(kColorPaymentsGooglePayLogo)),
            /*x=*/0, /*y=*/0, kGooglePayLogoWidth, kIconHeight);
        break;
      case TitleWithIconAndSeparatorView::Icon::GOOGLE_G:
        image =
            gfx::CreateVectorIcon(kGoogleGLogoIcon, kIconHeight,
                                  GetColorProvider()->GetColor(ui::kColorIcon));
        break;
    }

#else
    gfx::ImageSkia image =
        gfx::CreateVectorIcon(kCreditCardIcon, kIconHeight,
                              GetColorProvider()->GetColor(ui::kColorIcon));
#endif
    SetImage(image);
  }

 private:
  TitleWithIconAndSeparatorView::Icon icon_to_show_;
};

BEGIN_METADATA(IconView, views::ImageView)
END_METADATA

}  // namespace

TitleWithIconAndSeparatorView::TitleWithIconAndSeparatorView(
    const std::u16string& window_title,
    Icon icon_to_show) {
  AddColumn(views::LayoutAlignment::kStart, views::LayoutAlignment::kStart,
            views::TableLayout::kFixedSize,
            views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(views::LayoutAlignment::kStart, views::LayoutAlignment::kStart,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(views::LayoutAlignment::kStretch,
                 views::LayoutAlignment::kStart, 1.0f,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, views::TableLayout::kFixedSize);

  auto* icon_view_ptr = AddChildView(std::make_unique<IconView>(icon_to_show));

  auto separator = std::make_unique<views::Separator>();
  separator->SetPreferredLength(kSeparatorHeight);
  auto* separator_ptr = AddChildView(std::move(separator));

  auto title_label = std::make_unique<views::Label>(
      window_title, views::style::CONTEXT_DIALOG_TITLE);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);
  auto* title_label_ptr = AddChildView(std::move(title_label));

  // Add vertical padding to the icon and the separator so they are aligned with
  // the first line of title label. This needs to be done after we create the
  // title label, so that we can use its preferred size.
  const int title_label_height = title_label_ptr->GetPreferredSize().height();
  icon_view_ptr->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR((title_label_height - kIconHeight) / 2, 0, 0, 0)));
  // TODO(crbug.com/873140): DISTANCE_RELATED_BUTTON_HORIZONTAL isn't the right
  //                         choice here, but INSETS_DIALOG_TITLE gives too much
  //                         padding. Create a new Harmony DistanceMetric?
  const int separator_horizontal_padding =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  separator_ptr->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      (title_label_height - kSeparatorHeight) / 2, separator_horizontal_padding,
      0, separator_horizontal_padding)));
}

TitleWithIconAndSeparatorView::~TitleWithIconAndSeparatorView() {}

gfx::Size TitleWithIconAndSeparatorView::GetMinimumSize() const {
  // View::GetMinimum() defaults to GridLayout::GetPreferredSize(), but that
  // gives a larger frame width, so the dialog will become wider than it should.
  // To avoid that, just return 0x0.
  return gfx::Size(0, 0);
}

BEGIN_METADATA(TitleWithIconAndSeparatorView, views::View)
END_METADATA

std::unique_ptr<views::Textfield> CreateCvcTextfield() {
  auto textfield = std::make_unique<views::Textfield>();
  textfield->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC));
  textfield->SetDefaultWidthInChars(8);
  textfield->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_NUMBER);
  return textfield;
}

LegalMessageView::LegalMessageView(const LegalMessageLines& legal_message_lines,
                                   LinkClickedCallback callback) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetBetweenChildSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL));
  for (const LegalMessageLine& line : legal_message_lines) {
    views::StyledLabel* label =
        AddChildView(std::make_unique<views::StyledLabel>());
    label->SetText(line.text());
    label->SetTextContext(CONTEXT_DIALOG_BODY_TEXT_SMALL);
    label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
    for (const LegalMessageLine::Link& link : line.links()) {
      label->AddStyleRange(link.range,
                           views::StyledLabel::RangeStyleInfo::CreateForLink(
                               base::BindRepeating(callback, link.url)));
    }
  }
}

LegalMessageView::~LegalMessageView() = default;

BEGIN_METADATA(LegalMessageView, views::View)
END_METADATA

PaymentsBubbleClosedReason GetPaymentsBubbleClosedReasonFromWidget(
    const views::Widget* widget) {
  DCHECK(widget);
  if (!widget->IsClosed())
    return PaymentsBubbleClosedReason::kUnknown;

  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kUnspecified:
      return PaymentsBubbleClosedReason::kNotInteracted;
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
      return PaymentsBubbleClosedReason::kClosed;
    case views::Widget::ClosedReason::kLostFocus:
      return PaymentsBubbleClosedReason::kLostFocus;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      return PaymentsBubbleClosedReason::kAccepted;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      return PaymentsBubbleClosedReason::kCancelled;
  }
}

ProgressBarWithTextView::ProgressBarWithTextView(
    const std::u16string& progress_bar_text) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetBetweenChildSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL));
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  progress_throbber_ = AddChildView(std::make_unique<views::Throbber>());
  progress_label_ =
      AddChildView(std::make_unique<views::Label>(progress_bar_text));
}

ProgressBarWithTextView::~ProgressBarWithTextView() = default;

void ProgressBarWithTextView::OnThemeChanged() {
  views::View::OnThemeChanged();

  // We need to ensure |progress_label_|'s color matches the color of the
  // throbber above it.
  progress_label_->SetEnabledColor(
      GetColorProvider()->GetColor(ui::kColorThrobber));
}

void ProgressBarWithTextView::AddedToWidget() {
  progress_throbber_->Start();
}

BEGIN_METADATA(ProgressBarWithTextView, views::View)
END_METADATA

}  // namespace autofill
