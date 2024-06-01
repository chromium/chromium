// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_disclaimer_view.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_constants.h"
#include "chrome/grit/component_extension_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace chromeos {

namespace {

constexpr char kWidgetName[] = "MagicBoostDisclaimerViewWidget";

// Paddings, sizes and insets.
constexpr int kImageWidth = 512;
constexpr int kContainerPadding = 32;
constexpr int kTextContainerBetweenChildSpacing = 16;
constexpr int kContainerBottomPadding = 28;
constexpr int kWidgetWidth = kImageWidth;
constexpr int kWidgetHeight = 650;
constexpr int kBetweenButtonsSpacing = 8;
constexpr int kButtonHeight = 32;
constexpr gfx::Insets kButtonContainerInsets =
    gfx::Insets::TLBR(0,
                      kContainerPadding,
                      kContainerBottomPadding,
                      kContainerPadding);
constexpr gfx::Insets kTextContainerInsets = gfx::Insets(kContainerPadding);
constexpr gfx::Size kImagePreferredSize(/*width=*/kImageWidth, /*height=*/236);

// Font lists
const gfx::FontList body_text_font_list =
    gfx::FontList({"Google Sans", "Roboto"},
                  gfx::Font::NORMAL,
                  /*font_size=*/14,
                  gfx::Font::Weight::NORMAL);
const gfx::FontList title_text_font_list =
    gfx::FontList({"Google Sans", "Roboto"},
                  gfx::Font::NORMAL,
                  /*font_size=*/18,
                  gfx::Font::Weight::MEDIUM);

views::StyledLabel::RangeStyleInfo GetBodyTextStyle() {
  views::StyledLabel::RangeStyleInfo style;
  style.custom_font = body_text_font_list;
  style.override_color_id = static_cast<ui::ColorId>(ui::kColorSysOnSurface);
  return style;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Placeholder url.
constexpr char kTestURL[] = "https://www.google.com";

// Opens the passed in `url` in a new tab.
void OnLinkClick(const std::string& url) {
  // TODO(b/339044721): open the url in a new tab.
}

views::StyledLabel::RangeStyleInfo GetLinkTextStyle() {
  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          base::BindRepeating(&OnLinkClick, kTestURL));
  link_style.override_color_id = static_cast<ui::ColorId>(ui::kColorSysPrimary);
  return link_style;
}
#else
// Placeholder texts
// TODO(b/339528642): Replace with real strings.
const std::u16string kTestTitleText = u"Disclaimer title";
const std::u16string kTestSecondaryButtonText = u"No thanks";
const std::u16string kTestPrimaryButtonText = u"Try it";
const std::u16string kTestBodyText =
    u"Body text that is multi-line which means it can span from one line to up "
    u"to three lines for this case.";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

views::Builder<views::StyledLabel> GetTextBodyBuilder(
    const std::u16string& text) {
  return views::Builder<views::StyledLabel>()
      .SetText(text)
      .AddStyleRange(gfx::Range(0, text.length()), GetBodyTextStyle())
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false);
}

std::u16string GetTitle() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_MAGIC_BOOST_DISCLAIMER_TITLE);
#else
  return kTestTitleText;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetAcceptButtonText() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_MAGIC_BOOST_DISCLAIMER_ACCEPT_BUTTON);
#else
  return kTestPrimaryButtonText;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetDeclineButtonText() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_MAGIC_BOOST_DISCLAIMER_DECLINE_BUTTON);
#else
  return kTestSecondaryButtonText;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

views::Builder<views::StyledLabel> GetParagraphOneBuilder() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return GetTextBodyBuilder(
      l10n_util::GetStringUTF16(IDS_MAGIC_BOOST_DISCLAMIER_PARAGRAPH_ONE));
#else
  return GetTextBodyBuilder(kTestBodyText);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

views::Builder<views::StyledLabel> GetParagraphTwoBuilder() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::vector<size_t> offsets;
  const std::u16string link_text =
      l10n_util::GetStringUTF16(IDS_MAGIC_BOOST_DISCLAIMER_TERMS_LINK_TEXT);
  const std::u16string text = l10n_util::GetStringFUTF16(
      IDS_MAGIC_BOOST_DISCLAIMER_PARAGRAPH_TWO, {link_text}, &offsets);

  return views::Builder<views::StyledLabel>()
      .SetText(text)
      .AddStyleRange(gfx::Range(0, offsets.at(0)), GetBodyTextStyle())
      .AddStyleRange(
          gfx::Range(offsets.at(0), offsets.at(0) + link_text.length()),
          GetLinkTextStyle())
      .AddStyleRange(
          gfx::Range(offsets.at(0) + link_text.length(), text.length()),
          GetBodyTextStyle())
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false);

#else
  return GetTextBodyBuilder(kTestBodyText);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

views::Builder<views::StyledLabel> GetParagraphThreeBuilder() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return GetTextBodyBuilder(
      l10n_util::GetStringUTF16(IDS_MAGIC_BOOST_DISCLAIMER_PARAGRAPH_THREE));
#else
  return GetTextBodyBuilder(kTestBodyText);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

views::Builder<views::StyledLabel> GetParagraphFourBuilder() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::vector<size_t> offsets;
  const std::u16string link_text = l10n_util::GetStringUTF16(
      IDS_MAGIC_BOOST_DISCLAIMER_LEARN_MORE_LINK_TEXT);
  const std::u16string text = l10n_util::GetStringFUTF16(
      IDS_MAGIC_BOOST_DISCLAIMER_PARAGRAPH_FOUR, {link_text}, &offsets);

  return views::Builder<views::StyledLabel>()
      .SetText(text)
      .AddStyleRange(gfx::Range(0, offsets.at(0)), GetBodyTextStyle())
      .AddStyleRange(
          gfx::Range(offsets.at(0), offsets.at(0) + link_text.length()),
          GetLinkTextStyle())
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetAutoColorReadabilityEnabled(false);
#else
  return GetTextBodyBuilder(kTestBodyText);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace

MagicBoostDisclaimerView::MagicBoostDisclaimerView()
    : chromeos::editor_menu::PreTargetHandlerView(
          chromeos::editor_menu::CardType::kMahiDefaultMenu) {
  views::Builder<views::View>(this)
      .SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      .SetBackground(views::CreateThemedRoundedRectBackground(
          ui::kColorSysBaseContainerElevated,
          views::LayoutProvider::Get()->GetCornerRadiusMetric(
              views::ShapeContextTokens::kMenuRadius)))
      .AddChildren(
          views::Builder<views::ImageView>()
              .SetImage(
                  ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                      IDR_MAGIC_BOOST_DISCLAIMER_IMAGE))
              .SetPreferredSize(kImagePreferredSize),
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::LayoutOrientation::kVertical)
              .SetProperty(views::kBoxLayoutFlexKey,
                           views::BoxLayoutFlexSpecification())
              .SetBetweenChildSpacing(kTextContainerBetweenChildSpacing)
              .SetBorder(views::CreateEmptyBorder(kTextContainerInsets))
              .AddChildren(views::Builder<views::Label>()
                               .SetFontList(title_text_font_list)
                               .SetEnabledColorId(ui::kColorSysOnSurface)
                               .SetHorizontalAlignment(
                                   gfx::HorizontalAlignment::ALIGN_LEFT)
                               .SetText(GetTitle()),
                           GetParagraphOneBuilder(), GetParagraphTwoBuilder(),
                           GetParagraphThreeBuilder(),
                           GetParagraphFourBuilder()),
          views::Builder<views::BoxLayoutView>()
              .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
              .SetBetweenChildSpacing(kBetweenButtonsSpacing)
              .SetBorder(views::CreateEmptyBorder(kButtonContainerInsets))
              .AddChildren(
                  views::Builder<views::MdTextButton>()
                      .SetText(GetDeclineButtonText())
                      .SetID(magic_boost::ViewId::DisclaimerViewDeclineButton)
                      .SetAccessibleName(GetDeclineButtonText())
                      // Sets the button's height to a customized
                      // `kButtonHeight` instead of using the default height.
                      .SetMaxSize(gfx::Size(kImageWidth, kButtonHeight))
                      .SetStyle(ui::ButtonStyle::kProminent)
                      .SetCallback(base::BindRepeating(
                          &MagicBoostDisclaimerView::OnDeclineButtonPressed,
                          weak_ptr_factory_.GetWeakPtr())),
                  views::Builder<views::MdTextButton>()
                      .CopyAddressTo(&accept_button_)
                      .SetID(magic_boost::ViewId::DisclaimerViewAcceptButton)
                      .SetText(GetAcceptButtonText())
                      .SetAccessibleName(GetAcceptButtonText())
                      .SetMaxSize(gfx::Size(kImageWidth, kButtonHeight))
                      .SetStyle(ui::ButtonStyle::kProminent)
                      .SetCallback(base::BindRepeating(
                          &MagicBoostDisclaimerView::OnAcceptButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()))))
      .BuildChildren();
}

MagicBoostDisclaimerView::~MagicBoostDisclaimerView() = default;

// static
views::UniqueWidgetPtr MagicBoostDisclaimerView::CreateWidget() {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.name = GetWidgetName();

  views::UniqueWidgetPtr widget =
      std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<MagicBoostDisclaimerView>());

  // Shows the widget in the middle of the screen.
  // TODO(b/339044721): Set the widget bounds based on different screen size.
  auto bounds = display::Screen::GetScreen()
                    ->GetPrimaryDisplay()
                    .work_area()
                    .CenterPoint();
  widget->SetBounds(gfx::Rect(bounds.x() - kWidgetWidth / 2,
                              bounds.y() - kWidgetHeight / 2, kWidgetWidth,
                              kWidgetHeight));

  return widget;
}

// static
const char* MagicBoostDisclaimerView::GetWidgetName() {
  return kWidgetName;
}

void MagicBoostDisclaimerView::RequestFocus() {
  views::View::RequestFocus();

  accept_button_->RequestFocus();
}

void MagicBoostDisclaimerView::OnAcceptButtonPressed() {
  auto* controller = MagicBoostCardController::Get();
  if (controller->is_orca_included()) {
    controller->SetAllFeaturesState(true);
  } else {
    controller->SetQuickAnswersAndMahiFeaturesState(true);
  }

  // TODO(b/339044721): Implement accept action: showing the next view etc.
  controller->CloseDisclaimerUi();
}

void MagicBoostDisclaimerView::OnDeclineButtonPressed() {
  auto* controller = MagicBoostCardController::Get();
  if (controller->is_orca_included()) {
    controller->SetAllFeaturesState(false);
  } else {
    controller->SetQuickAnswersAndMahiFeaturesState(false);
  }
  controller->CloseDisclaimerUi();
}

BEGIN_METADATA(MagicBoostDisclaimerView)
END_METADATA

}  // namespace chromeos
