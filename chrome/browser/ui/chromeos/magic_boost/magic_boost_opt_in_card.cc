// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_opt_in_card.h"

#include <string>

#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_card_controller.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_constants.h"
#include "chrome/browser/ui/chromeos/magic_boost/magic_boost_metrics.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

// Widget constants
constexpr char kWidgetName[] = "MagicBoostOptInWidget";

// Card constants
constexpr gfx::Insets kInteriorMargin = gfx::Insets(16);
constexpr float kCornerRadius = 8.0f;

// Label constants
constexpr int kTitleLabelMaxLines = 2;
constexpr int kBodyLabelMaxLines = 3;

// Image constants
constexpr int kImageViewSize = 36;
constexpr int kImageViewCornerRadius = 12;
constexpr int kImageViewIconSize = 20;

// Button constants
constexpr int kButtonsContainerHeight = 32;

// Spacing constants
constexpr int kBetweenButtonsSpacing = 8;
constexpr int kBetweenImageAndTextSpacing = 16;
constexpr int kBetweenContentsAndButtonsSpacing = 16;
constexpr int kBetweenLabelsSpacing = 4;

// Font lists
const gfx::FontList kBodyTextFontList =
    gfx::FontList({"Google Sans", "Roboto"},
                  gfx::Font::NORMAL,
                  /*font_size=*/12,
                  gfx::Font::Weight::NORMAL);
const gfx::FontList kTitleTextFontList =
    gfx::FontList({"Google Sans", "Roboto"},
                  gfx::Font::NORMAL,
                  /*font_size=*/14,
                  gfx::Font::Weight::MEDIUM);

}  // namespace

// MagicBoostOptInCard --------------------------------------------------------

MagicBoostOptInCard::MagicBoostOptInCard(MagicBoostCardController* controller)
    : chromeos::editor_menu::PreTargetHandlerView(
          /*card_type=*/editor_menu::CardType::kMagicBoostOptInCard),
      controller_(controller) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(kInteriorMargin)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::VH(kBetweenContentsAndButtonsSpacing, 0))
      .SetCollapseMargins(true)
      .SetIgnoreDefaultMainAxisMargins(true);
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorPrimaryBackground));

  // Painted to layer so view can be semi-transparent and set rounded corners.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kCornerRadius));

  // Create image and text container and add an image view.
  auto* image_and_text_container = AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
          .CustomConfigure(base::BindOnce([](views::FlexLayoutView* layout) {
            layout->SetDefault(views::kMarginsKey,
                               gfx::Insets::VH(0, kBetweenImageAndTextSpacing));
          }))
          .SetCollapseMargins(true)
          .SetIgnoreDefaultMainAxisMargins(true)
          // Set FlexSpecification to `kUnbounded` so the body text can take up
          // more height when it's multi-line.
          .SetProperty(
              views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                       views::MaximumFlexSizeRule::kUnbounded,
                                       /*adjust_height_for_width=*/true))
          .AddChildren(
              views::Builder<views::ImageView>()
                  .SetPreferredSize(gfx::Size(kImageViewSize, kImageViewSize))
                  .SetImage(ui::ImageModel::FromVectorIcon(
                      kMahiSparkIcon, ui::kColorSysOnPrimaryContainer,
                      kImageViewIconSize))
                  .SetBackground(views::CreateThemedSolidBackground(
                      ui::kColorSysPrimaryContainer))
                  // Painted to layer to set rounded corners.
                  .SetPaintToLayer()
                  .CustomConfigure(
                      base::BindOnce([](views::ImageView* image_view) {
                        image_view->layer()->SetFillsBoundsOpaquely(false);
                        image_view->layer()->SetRoundedCornerRadius(
                            gfx::RoundedCornersF(kImageViewCornerRadius));
                      })))
          .Build());

  // Create text container that holds title and body text.
  bool include_orca =
      controller_->GetOptInFeatures() == OptInFeatures::kOrcaAndHmr;
  image_and_text_container->AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .CustomConfigure(base::BindOnce([](views::FlexLayoutView* layout) {
            layout->SetDefault(views::kMarginsKey,
                               gfx::Insets::VH(kBetweenLabelsSpacing, 0));
          }))
          .SetCollapseMargins(true)
          .SetIgnoreDefaultMainAxisMargins(true)
          // Set FlexSpecification to `kUnbounded` so the body text can
          // take up more height when it's multi-line.
          .SetProperty(
              views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded,
                                       /*adjust_height_for_width=*/true))
          .AddChildren(
              views::Builder<views::Label>()
                  .CopyAddressTo(&title_label_)
                  .SetID(magic_boost::ViewId::OptInCardTitleLabel)
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_ASH_MAGIC_BOOST_OPT_IN_CARD_TITLE))
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetEnabledColorId(ui::kColorSysOnSurface)
                  .SetAutoColorReadabilityEnabled(false)
                  .SetSubpixelRenderingEnabled(false)
                  .SetText(l10n_util::GetStringUTF16(
                      include_orca
                          ? IDS_ASH_MAGIC_BOOST_OPT_IN_CARD_TITLE
                          : IDS_ASH_MAGIC_BOOST_OPT_IN_CARD_NO_ORCA_TITLE))
                  .SetFontList(kTitleTextFontList)
                  .SetMultiLine(true)
                  .SetMaxLines(kTitleLabelMaxLines),
              views::Builder<views::Label>()
                  .CopyAddressTo(&body_label_)
                  .SetID(magic_boost::ViewId::OptInCardBodyLabel)
                  .SetText(l10n_util::GetStringUTF16(
                      IDS_ASH_MAGIC_BOOST_OPT_IN_CARD_BODY))
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetEnabledColorId(ui::kColorSysOnSurface)
                  .SetAutoColorReadabilityEnabled(false)
                  .SetSubpixelRenderingEnabled(false)
                  .SetText(l10n_util::GetStringUTF16(
                      include_orca
                          ? IDS_ASH_MAGIC_BOOST_OPT_IN_CARD_BODY
                          : IDS_ASH_MAGIC_BOOST_OPT_IN_CARD_NO_ORCA_BODY))
                  .SetFontList(kBodyTextFontList)
                  .SetMultiLine(true)
                  .SetMaxLines(kBodyLabelMaxLines))
          .Build());

  // Create buttons container that holds two buttons.
  std::u16string decline_button_text =
      l10n_util::GetStringUTF16(IDS_ASH_MAGIC_BOOST_OPT_IN_CARD_DECLINE_BUTTON);
  std::u16string accept_button_text =
      l10n_util::GetStringUTF16(IDS_ASH_MAGIC_BOOST_OPT_IN_CARD_ACCEPT_BUTTON);
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
          .SetBetweenChildSpacing(kBetweenButtonsSpacing)
          // Set a preferred size so buttons can adjust to the desired height,
          // instead of the default height set by the `MdTextButton` class.
          .SetPreferredSize(gfx::Size(image_and_text_container->width(),
                                      kButtonsContainerHeight))
          .AddChildren(views::Builder<views::MdTextButton>()
                           .CopyAddressTo(&secondary_button_)
                           .SetID(magic_boost::ViewId::OptInCardSecondaryButton)
                           .SetText(decline_button_text)
                           .SetAccessibleName(decline_button_text)
                           .SetStyle(ui::ButtonStyle::kText)
                           .SetCallback(base::BindRepeating(
                               &MagicBoostOptInCard::OnSecondaryButtonPressed,
                               weak_ptr_factory_.GetWeakPtr())),
                       views::Builder<views::MdTextButton>()
                           .SetID(magic_boost::ViewId::OptInCardPrimaryButton)
                           .SetText(accept_button_text)
                           .SetAccessibleName(accept_button_text)
                           .SetStyle(ui::ButtonStyle::kProminent)
                           .SetCallback(base::BindRepeating(
                               &MagicBoostOptInCard::OnPrimaryButtonPressed,
                               weak_ptr_factory_.GetWeakPtr())))
          .Build());
}

MagicBoostOptInCard::~MagicBoostOptInCard() = default;

// static
views::UniqueWidgetPtr MagicBoostOptInCard::CreateWidget(
    MagicBoostCardController* controller,
    const gfx::Rect& anchor_view_bounds) {
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
  MagicBoostOptInCard* magic_boost_opt_in_card = widget->SetContentsView(
      std::make_unique<MagicBoostOptInCard>(controller));
  magic_boost_opt_in_card->UpdateWidgetBounds(anchor_view_bounds);

  return widget;
}

// static
const char* MagicBoostOptInCard::GetWidgetName() {
  return kWidgetName;
}

void MagicBoostOptInCard::UpdateWidgetBounds(
    const gfx::Rect& anchor_view_bounds) {
  // TODO(b/318733414): Move `GetEditorMenuBounds` to a common place to use.
  GetWidget()->SetBounds(
      editor_menu::GetEditorMenuBounds(anchor_view_bounds, this));
}

void MagicBoostOptInCard::RequestFocus() {
  views::View::RequestFocus();
  secondary_button_->RequestFocus();
}

// static
const char* MagicBoostOptInCard::GetWidgetNameForTest() {
  return kWidgetName;
}

void MagicBoostOptInCard::OnPrimaryButtonPressed() {
  magic_boost::RecordOptInCardActionMetrics(
      controller_->GetOptInFeatures(),
      magic_boost::OptInCardAction::kAcceptButtonPressed);

  controller_->CloseOptInUi();

  controller_->ShowDisclaimerUi(/*display_id=*/
                                display::Screen::GetScreen()
                                    ->GetDisplayNearestWindow(
                                        GetWidget()->GetNativeWindow())
                                    .id());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* magic_boost_state = chromeos::MagicBoostState::Get();
  magic_boost_state->AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kPendingDisclaimer);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void MagicBoostOptInCard::OnSecondaryButtonPressed() {
  magic_boost::RecordOptInCardActionMetrics(
      controller_->GetOptInFeatures(),
      magic_boost::OptInCardAction::kDeclineButtonPressed);

  controller_->CloseOptInUi();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* magic_boost_state = chromeos::MagicBoostState::Get();
  if (controller_->GetOptInFeatures() == OptInFeatures::kOrcaAndHmr) {
    magic_boost_state->DisableOrcaFeature();
  }
  magic_boost_state->AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kDeclined);
  magic_boost_state->AsyncWriteHMREnabled(/*enabled=*/false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

BEGIN_METADATA(MagicBoostOptInCard)
END_METADATA

}  // namespace chromeos
