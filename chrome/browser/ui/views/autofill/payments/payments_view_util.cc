// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/payments_view_util.h"

#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"

namespace autofill {

namespace {

// Dimensions of the Google Pay logo.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr int kGooglePayLogoWidth = 40;
#endif
constexpr int kGooglePayLogoHeight = 16;

constexpr int kGooglePayLogoSeparatorHeight = 12;

constexpr SkColor kTitleSeparatorColor = SkColorSetRGB(0x9E, 0x9E, 0x9E);

}  // namespace

TitleWithIconAndSeparatorView::TitleWithIconAndSeparatorView(
    const base::string16& window_title) {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* columns = layout->AddColumnSet(0);

  // Add columns for the Google Pay icon, the separator, and the title label.
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, 0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                     views::GridLayout::kFixedSize, views::GridLayout::USE_PREF,
                     0, 0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING, 1.f,
                     views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(views::GridLayout::kFixedSize, 0);

  auto icon_view = std::make_unique<views::ImageView>();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // kGooglePayLogoIcon is square, and CreateTiledImage() will clip it whereas
  // setting the icon size would rescale it incorrectly.
  gfx::ImageSkia image = gfx::ImageSkiaOperations::CreateTiledImage(
      gfx::CreateVectorIcon(kGooglePayLogoIcon,
                            GetNativeTheme()->ShouldUseDarkColors()
                                ? gfx::kGoogleGrey200
                                : gfx::kGoogleGrey700),
      /*x=*/0, /*y=*/0, kGooglePayLogoWidth, kGooglePayLogoHeight);
#else
  gfx::ImageSkia image =
      gfx::CreateVectorIcon(kCreditCardIcon, kGooglePayLogoHeight,
                            GetNativeTheme()->GetSystemColor(
                                ui::NativeTheme::kColorId_DefaultIconColor));
#endif
  icon_view->SetImage(image);
  auto* icon_view_ptr = layout->AddView(std::move(icon_view));

  auto separator = std::make_unique<views::Separator>();
  separator->SetColor(kTitleSeparatorColor);
  separator->SetPreferredHeight(kGooglePayLogoSeparatorHeight);
  auto* separator_ptr = layout->AddView(std::move(separator));

  auto title_label = std::make_unique<views::Label>(
      window_title, views::style::CONTEXT_DIALOG_TITLE);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);
  auto* title_label_ptr = layout->AddView(std::move(title_label));

  // Add vertical padding to the icon and the separator so they are aligned with
  // the first line of title label. This needs to be done after we create the
  // title label, so that we can use its preferred size.
  const int title_label_height = title_label_ptr->GetPreferredSize().height();
  icon_view_ptr->SetBorder(views::CreateEmptyBorder(
      /*top=*/(title_label_height - kGooglePayLogoHeight) / 2,
      /*left=*/0, /*bottom=*/0, /*right=*/0));
  // TODO(crbug.com/873140): DISTANCE_RELATED_BUTTON_HORIZONTAL isn't the right
  //                         choice here, but INSETS_DIALOG_TITLE gives too much
  //                         padding. Create a new Harmony DistanceMetric?
  const int separator_horizontal_padding =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_BUTTON_HORIZONTAL);
  separator_ptr->SetBorder(views::CreateEmptyBorder(
      /*top=*/(title_label_height - kGooglePayLogoSeparatorHeight) / 2,
      /*left=*/separator_horizontal_padding, /*bottom=*/0,
      /*right=*/separator_horizontal_padding));
}

TitleWithIconAndSeparatorView::~TitleWithIconAndSeparatorView() {}

gfx::Size TitleWithIconAndSeparatorView::GetMinimumSize() const {
  // View::GetMinimum() defaults to GridLayout::GetPreferredSize(), but that
  // gives a larger frame width, so the dialog will become wider than it should.
  // To avoid that, just return 0x0.
  return gfx::Size(0, 0);
}

std::unique_ptr<views::Textfield> CreateCvcTextfield() {
  auto textfield = std::make_unique<views::Textfield>();
  textfield->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC));
  textfield->SetDefaultWidthInChars(8);
  textfield->SetTextInputType(ui::TextInputType::TEXT_INPUT_TYPE_NUMBER);
  return textfield;
}

LegalMessageView::LegalMessageView(const LegalMessageLines& legal_message_lines,
                                   views::StyledLabelListener* listener)
    : legal_message_lines_(legal_message_lines) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  for (const LegalMessageLine& line : legal_message_lines) {
    AddChildView(CreateLegalMessageLineLabel(line, listener).release());
  }
}

LegalMessageView::~LegalMessageView() {}

std::unique_ptr<views::StyledLabel>
LegalMessageView::CreateLegalMessageLineLabel(
    const LegalMessageLine& line,
    views::StyledLabelListener* listener) {
  std::unique_ptr<views::StyledLabel> label =
      std::make_unique<views::StyledLabel>(line.text(), listener);
  label->SetTextContext(CONTEXT_BODY_TEXT_LARGE);
  label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  for (const LegalMessageLine::Link& link : line.links()) {
    label->AddStyleRange(link.range,
                         views::StyledLabel::RangeStyleInfo::CreateForLink());
  }
  return label;
}

const GURL LegalMessageView::GetUrlForLink(views::StyledLabel* label,
                                           const gfx::Range& range) {
  // Index of |label| within its parent's view hierarchy is the same as the
  // legal message line index. DCHECK this assumption to guard against future
  // layout changes.
  DCHECK_EQ(label->parent()->children().size(), legal_message_lines_.size());

  const std::vector<LegalMessageLine::Link>& links =
      legal_message_lines_[label->parent()->GetIndexOf(label)].links();
  for (const LegalMessageLine::Link& link : links) {
    if (link.range == range) {
      return link.url;
    }
  }
  // |range| was not found.
  NOTREACHED();
  return GURL();
}

}  // namespace autofill
