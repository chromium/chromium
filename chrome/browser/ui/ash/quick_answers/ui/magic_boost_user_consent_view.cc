// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_answers/ui/magic_boost_user_consent_view.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/ui/ash/editor_menu/utils/pre_target_handler.h"
#include "chrome/browser/ui/ash/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/ash/quick_answers/ui/magic_boost_header.h"
#include "chrome/browser/ui/ash/quick_answers/ui/quick_answers_util.h"
#include "chrome/browser/ui/ash/quick_answers/ui/typography.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_view.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace quick_answers {

namespace {

// Main view (or common) specs.
constexpr auto kButtonPadding = gfx::Insets::VH(6, 8);
constexpr auto kMainViews = gfx::Insets::TLBR(12, 16, 12, 14);
constexpr auto kHeaderInsets = gfx::Insets::TLBR(0, 0, 8, 0);
constexpr auto kSparkIconInsets = gfx::Insets::TLBR(0, 0, 0, 4);

constexpr int kButtonBorderThickness = 1;
constexpr int kButtonCornerRadius = 8;
constexpr int kSettingsButtonSizeDip = 16;
constexpr int kSettingsButtonBorderDip = 3;

// Icon.
constexpr gfx::Insets kIntentIconInsets = gfx::Insets(0);

std::u16string GetChipLabel(IntentType intent_type,
                            const std::u16string& intent_text) {
  switch (intent_type) {
    case IntentType::kUnit:
      return l10n_util::GetStringFUTF16(
          IDS_QUICK_ANSWERS_MAGIC_BOOST_USER_CONSENT_UNIT_CONVERSION_INTENT_LABEL_BUTTON,
          intent_text);
    case IntentType::kDictionary:
      return l10n_util::GetStringFUTF16(
          IDS_QUICK_ANSWERS_MAGIC_BOOST_USER_CONSENT_DEFINITION_INTENT_LABEL_BUTTON,
          intent_text);
    case IntentType::kTranslation:
      return l10n_util::GetStringFUTF16(
          IDS_QUICK_ANSWERS_MAGIC_BOOST_USER_CONSENT_TRANSLATION_INTENT_LABEL_BUTTON,
          intent_text);
    case IntentType::kUnknown:
      NOTREACHED() << "No chip label for Unknown intent type";
  }
  NOTREACHED() << "Unknown enum";
}

}  // namespace

// MagicBoostUserConsentView
// -------------------------------------------------------------

MagicBoostUserConsentView::MagicBoostUserConsentView(
    IntentType intent_type,
    const std::u16string& intent_text,
    chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller)
    : chromeos::ReadWriteCardsView(read_write_cards_ui_controller),
      focus_search_(
          this,
          base::BindRepeating(&MagicBoostUserConsentView::GetFocusableViews,
                              base::Unretained(this))) {
  SetUseDefaultFillLayout(true);
  SetBackground(views::CreateSolidBackground(ui::kColorPrimaryBackground));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->set_inside_border_insets(kMainViews);

  // Adds the header row
  AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetProperty(views::kMarginsKey, kHeaderInsets)
          .AddChild(
              views::Builder<views::BoxLayoutView>()
                  .SetOrientation(views::LayoutOrientation::kHorizontal)
                  .SetMainAxisAlignment(views::LayoutAlignment::kStart)
                  .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::LayoutOrientation::kHorizontal,
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded))
                  .AddChild(
                      views::Builder<views::ImageView>()
                          .SetImage(ui::ImageModel::FromVectorIcon(
                              chromeos::kInfoSparkIcon,
                              ui::ColorIds::kColorSysOnSurface, kIconSizeDip))
                          .SetProperty(views::kMarginsKey, kSparkIconInsets))
                  .AddChild(GetMagicBoostHeader()))
          .AddChild(
              views::Builder<views::ImageButton>()
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_RICH_ANSWERS_VIEW_SETTINGS_BUTTON_A11Y_NAME_TEXT))
                  .CopyAddressTo(&settings_button_)
                  .SetImageModel(
                      views::Button::ButtonState::STATE_NORMAL,
                      ui::ImageModel::FromVectorIcon(
                          vector_icons::kSettingsOutlineIcon,
                          ui::kColorSysSecondary, kSettingsButtonSizeDip))
                  .SetProperty(views::kMarginsKey,
                               gfx::Insets(kSettingsButtonBorderDip)))
          .Build());

  // Adds the chip row
  AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetInteriorMargin(kIntentIconInsets)
          .AddChild(
              views::Builder<views::LabelButton>()
                  .CopyAddressTo(&intent_chip_)
                  .SetText(GetChipLabel(intent_type, intent_text))
                  .SetLabelStyle(views::style::STYLE_BODY_4_EMPHASIS)
                  .SetTextColor(views::LabelButton::ButtonState::STATE_NORMAL,
                                ui::kColorSysOnSurface)
                  .SetTextColor(views::LabelButton::ButtonState::STATE_DISABLED,
                                ui::kColorSysStateDisabled)
                  .SetBorder(views::CreatePaddedBorder(
                      views::CreateRoundedRectBorder(kButtonBorderThickness,
                                                     kButtonCornerRadius,
                                                     ui::kColorSysTonalOutline),
                      kButtonPadding))
                  .SetEnabled(true))
          .Build());

  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetName(GetChipLabel(intent_type, intent_text));
  GetViewAccessibility().SetDescription(l10n_util::GetStringUTF16(
      IDS_QUICK_ANSWERS_MAGIC_BOOST_USER_NOTICE_VIEW_A11Y_INFO_DESC_TEXT));

  // Focus should cycle to each of the buttons the view contains and back to it.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
  views::FocusRing::Install(this);
}

MagicBoostUserConsentView::~MagicBoostUserConsentView() = default;

void MagicBoostUserConsentView::OnFocus() {}

views::FocusTraversable* MagicBoostUserConsentView::GetPaneFocusTraversable() {
  return &focus_search_;
}

void MagicBoostUserConsentView::UpdateBoundsForQuickAnswers() {}

std::u16string MagicBoostUserConsentView::chip_label_for_testing() {
  return intent_chip_ == nullptr ? u"" : intent_chip_->GetText().data();
}

std::vector<views::View*> MagicBoostUserConsentView::GetFocusableViews() {
  std::vector<views::View*> focusable_views;
  // The view itself is not included in focus loop, unless screen-reader is on.
  if (QuickAnswersState::Get()->spoken_feedback_enabled()) {
    focusable_views.push_back(this);
  }
  focusable_views.push_back(intent_chip_);
  focusable_views.push_back(settings_button_);
  return focusable_views;
}

void MagicBoostUserConsentView::SetSettingsButtonPressed(
    views::Button::PressedCallback callback) {
  settings_button_->SetCallback(std::move(callback));
}

void MagicBoostUserConsentView::SetIntentButtonPressedCallback(
    views::Button::PressedCallback callback) {
  intent_chip_->SetCallback(std::move(callback));
}

BEGIN_METADATA(MagicBoostUserConsentView)
END_METADATA

}  // namespace quick_answers
