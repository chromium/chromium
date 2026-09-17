// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_answers/ui/user_consent_view.h"

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/ui/ash/editor_menu/utils/pre_target_handler.h"
#include "chrome/browser/ui/ash/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/ash/quick_answers/ui/quick_answers_util.h"
#include "chrome/browser/ui/ash/quick_answers/ui/typography.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_view.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/common/content_switches.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
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
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace quick_answers {

namespace {

// Main view specs.
constexpr int kContentSpacingDip = 8;
constexpr auto kContentInsets = gfx::Insets::TLBR(0, 16, 0, 0);

// Icon.
constexpr int kIntentIconSizeDip = 20;
constexpr int kIconBackgroundCornerRadiusDip = 12;
constexpr gfx::Insets kIntentIconInsets = gfx::Insets(8);

// Label.
constexpr gfx::Insets kLabelMargin =
    gfx::Insets::TLBR(0, 0, kContentSpacingDip, 0);

// Buttons common.
constexpr int kButtonSpacingDip = 8;
constexpr auto kButtonBarInsets = gfx::Insets::TLBR(8, 0, 0, 0);

views::Builder<views::Label> GetConfiguredLabelBuilder(bool is_first_line) {
  return views::Builder<views::Label>()
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetLineHeight(is_first_line ? GetFirstLineHeight()
                                   : GetSecondLineHeight())
      .SetFontList(is_first_line ? GetFirstLineFontList()
                                 : GetSecondLineFontList());
}

// TODO(b/340628526): Use `quick_answers::Intent` in `UserConsentView`. For
// `IntentType::kUnknown`, it can be `std::nullopt` of `std::optional<Intent>`.
ResultType ToResultType(IntentType intent_type) {
  switch (intent_type) {
    case IntentType::kDictionary:
      return ResultType::kDefinitionResult;
    case IntentType::kTranslation:
      return ResultType::kTranslationResult;
    case IntentType::kUnit:
      return ResultType::kUnitConversionResult;
    case IntentType::kUnknown:
      return ResultType::kNoResult;
  }

  NOTREACHED() << "An invalid IntentType enum class value is provided";
}

std::optional<int> GetTitleMessageIdFor(IntentType intent_type) {
  switch (intent_type) {
    case IntentType::kDictionary:
      return IDS_QUICK_ANSWERS_USER_CONSENT_TITLE_DEFINITION_INTENT;
    case IntentType::kTranslation:
      return IDS_QUICK_ANSWERS_USER_CONSENT_TITLE_TRANSLATION_INTENT;
    case IntentType::kUnit:
      return IDS_QUICK_ANSWERS_USER_CONSENT_TITLE_UNIT_CONVERSION_INTENT;
    case IntentType::kUnknown:
      return std::nullopt;
  }

  NOTREACHED() << "An invalid IntentType enum class value is provided";
}

std::u16string GetTitle(IntentType intent_type,
                        const std::u16string& intent_text) {
  std::optional<int> message_id = GetTitleMessageIdFor(intent_type);
  if (!message_id.has_value() || intent_text.empty()) {
    // This is used only from Linux-ChromeOS, i.e., non-prod environment.
    return l10n_util::GetStringUTF16(
        IDS_QUICK_ANSWERS_USER_NOTICE_VIEW_TITLE_TEXT);
  }

  return l10n_util::GetStringFUTF16(message_id.value(), intent_text);
}

views::Builder<views::ImageView> GetIconFor(IntentType intent_type) {
  return views::Builder<views::ImageView>().SetImage(
      ui::ImageModel::FromVectorIcon(
          GetResultTypeIcon(ToResultType(intent_type)), ui::kColorSysOnSurface,
          kIntentIconSizeDip));
}

}  // namespace

// UserConsentView
// -------------------------------------------------------------

UserConsentView::UserConsentView(
    chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller)
    : chromeos::ReadWriteCardsView(read_write_cards_ui_controller),
      focus_search_(this,
                    base::BindRepeating(&UserConsentView::GetFocusableViews,
                                        base::Unretained(this))) {
  SetUseDefaultFillLayout(true);
  SetBackground(views::CreateSolidBackground(ui::kColorPrimaryBackground));

  views::FlexLayoutView* content;
  views::FlexLayoutView* buttons_container;

  // This is to avoid 80 char limit lint errors caused by long message ids and
  // indents.
  constexpr int kDescriptionMessageId =
      IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_DESCRIPTION_TEXT;
  constexpr int kNoThanksButtonMessageId =
      IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_NO_THANKS_BUTTON;
  constexpr int kTryItButtonMessageId =
      IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_TRY_IT_BUTTON;

  AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetInteriorMargin(kMainViewInsets)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
          .AddChild(views::Builder<views::FlexLayoutView>()
                        .SetBackground(views::CreateRoundedRectBackground(
                            ui::kColorSysPrimaryContainer,
                            kIconBackgroundCornerRadiusDip))
                        .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
                        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
                        .SetInteriorMargin(kIntentIconInsets)
                        .AddChild(GetIconFor(IntentType::kDictionary)
                                      .SetVisible(false)
                                      .CopyAddressTo(&dictionary_intent_icon_))
                        .AddChild(GetIconFor(IntentType::kTranslation)
                                      .SetVisible(false)
                                      .CopyAddressTo(&translation_intent_icon_))
                        .AddChild(GetIconFor(IntentType::kUnit)
                                      .SetVisible(false)
                                      .CopyAddressTo(&unit_intent_icon_))
                        .AddChild(GetIconFor(IntentType::kUnknown)
                                      .SetVisible(false)
                                      .CopyAddressTo(&unknown_intent_icon_)))
          .AddChild(
              views::Builder<views::FlexLayoutView>()
                  .CopyAddressTo(&content)
                  .SetOrientation(views::LayoutOrientation::kVertical)
                  .SetIgnoreDefaultMainAxisMargins(true)
                  .SetInteriorMargin(kContentInsets)
                  .SetCollapseMargins(true)
                  .AddChild(
                      GetConfiguredLabelBuilder(/*is_first_line=*/true)
                          .CopyAddressTo(&title_)
                          .SetEnabledColor(ui::kColorLabelForeground)
                          .SetProperty(views::kMarginsKey, kLabelMargin)
                          .SetProperty(
                              views::kFlexBehaviorKey,
                              views::FlexSpecification(
                                  views::MinimumFlexSizeRule::kScaleToMinimum,
                                  views::MaximumFlexSizeRule::kPreferred)))
                  .AddChild(
                      GetConfiguredLabelBuilder(/*is_first_line=*/false)
                          .CopyAddressTo(&description_)
                          .SetText(
                              l10n_util::GetStringUTF16(kDescriptionMessageId))
                          .SetMultiLine(true)
                          .SetEnabledColor(ui::kColorLabelForegroundSecondary)
                          .SetProperty(views::kMarginsKey, kLabelMargin)
                          .SetProperty(
                              views::kFlexBehaviorKey,
                              views::FlexSpecification(
                                  views::MinimumFlexSizeRule::kScaleToMinimum,
                                  views::MaximumFlexSizeRule::kPreferred,
                                  /*adjust_height_for_width=*/true)))
                  .AddChild(
                      views::Builder<views::FlexLayoutView>()
                          .CopyAddressTo(&buttons_container)
                          .SetOrientation(views::LayoutOrientation::kHorizontal)
                          .SetIgnoreDefaultMainAxisMargins(true)
                          .SetInteriorMargin(kButtonBarInsets)
                          .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
                          .SetCollapseMargins(true)
                          .CustomConfigure(
                              base::BindOnce([](views::FlexLayoutView* view) {
                                // `views::FlexLayoutView` does not have
                                // `SetDefault` for builder.
                                view->SetDefault(
                                    views::kMarginsKey,
                                    gfx::Insets::TLBR(0, 0, 0,
                                                      kButtonSpacingDip));
                              }))
                          .AddChild(
                              views::Builder<views::MdTextButton>()
                                  .CopyAddressTo(&no_thanks_button_)
                                  .SetText(l10n_util::GetStringUTF16(
                                      kNoThanksButtonMessageId))
                                  .SetCallback(base::BindRepeating(
                                      &QuickAnswersUiController::
                                          OnUserConsentResult,
                                      controller_, false))
                                  .SetStyle(ui::ButtonStyle::kText)
                                  // TODO(b/340628664): Consider if we can set
                                  // min size for `UserConsentView` itself. Use
                                  // MinimumFlexSizeRule=kPreferred instead of
                                  // `kScaleToZero`, etc to avoid making an
                                  // un-readable but actionable button. This is
                                  // to avoid showing following UI:
                                  //
                                  // Title
                                  // Description
                                  // [] []
                                  //
                                  // Two buttons are shown without text because
                                  // all button texts get truncated for
                                  // insufficient space.
                                  .SetProperty(views::kFlexBehaviorKey,
                                               views::FlexSpecification(
                                                   views::MinimumFlexSizeRule::
                                                       kPreferred,
                                                   views::MaximumFlexSizeRule::
                                                       kPreferred)))
                          .AddChild(
                              views::Builder<views::MdTextButton>()
                                  .CopyAddressTo(&allow_button_)
                                  .SetText(l10n_util::GetStringUTF16(
                                      kTryItButtonMessageId))
                                  .SetStyle(ui::ButtonStyle::kProminent)
                                  // TODO(b/340628664): Consider if we can set
                                  // min size for `UserConsentView` itself. Use
                                  // MinimumFlexSizeRule=kPreferred instead of
                                  // `kScaleToZero`, etc to avoid making an
                                  // un-readable but actionable button.
                                  .SetProperty(views::kFlexBehaviorKey,
                                               views::FlexSpecification(
                                                   views::MinimumFlexSizeRule::
                                                       kPreferred,
                                                   views::MaximumFlexSizeRule::
                                                       kPreferred)))))
          .Build());

  // Set preferred size of `button_bar` as a minimum x-axis size of `content`.
  // We intentionally let the layout overflow in x-axis. Without this,
  // `content` will try to render in the available size and end up in a wrong
  // height.
  CHECK(content);
  CHECK(buttons_container);
  content->SetMinimumCrossAxisSize(
      buttons_container->GetPreferredSize().width());

  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetDescription(l10n_util::GetStringFUTF8(
      IDS_QUICK_ANSWERS_USER_NOTICE_VIEW_A11Y_INFO_DESC_TEMPLATE,
      l10n_util::GetStringUTF16(kDescriptionMessageId)));

  UpdateIcon();
  UpdateUiText();

  // Focus should cycle to each of the buttons the view contains and back to it.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
  views::FocusRing::Install(this);
}

UserConsentView::~UserConsentView() = default;

std::optional<int> UserConsentView::GetMinWidth() const {
  return kMinWidth;
}

void UserConsentView::OnFocus() {
  // Unless screen-reader mode is enabled, transfer the focus to an actionable
  // button, otherwise retain to read out its contents.
  if (QuickAnswersState::Get()->spoken_feedback_enabled()) {
    no_thanks_button_->RequestFocus();
  }
}

views::FocusTraversable* UserConsentView::GetPaneFocusTraversable() {
  return &focus_search_;
}

void UserConsentView::UpdateBoundsForQuickAnswers() {
  // TODO(b/331271987): Remove this and the interface.
}

void UserConsentView::SetNoThanksButtonPressed(
    views::Button::PressedCallback callback) {
  no_thanks_button_->SetCallback(std::move(callback));
}

void UserConsentView::SetAllowButtonPressed(
    views::Button::PressedCallback callback) {
  allow_button_->SetCallback(std::move(callback));
}

void UserConsentView::SetIntentType(IntentType intent_type) {
  intent_type_ = intent_type;

  UpdateIcon();
  UpdateUiText();
}

void UserConsentView::SetIntentText(const std::u16string& intent_text) {
  intent_text_ = intent_text;

  UpdateUiText();
}

std::vector<views::View*> UserConsentView::GetFocusableViews() {
  std::vector<views::View*> focusable_views;
  // The view itself is not included in focus loop, unless screen-reader is on.
  if (QuickAnswersState::Get()->spoken_feedback_enabled()) {
    focusable_views.push_back(this);
  }
  focusable_views.push_back(no_thanks_button_);
  focusable_views.push_back(allow_button_);
  return focusable_views;
}

void UserConsentView::UpdateIcon() {
  dictionary_intent_icon_->SetVisible(intent_type_ == IntentType::kDictionary);
  translation_intent_icon_->SetVisible(intent_type_ ==
                                       IntentType::kTranslation);
  unit_intent_icon_->SetVisible(intent_type_ == IntentType::kUnit);
  unknown_intent_icon_->SetVisible(intent_type_ == IntentType::kUnknown);
}

void UserConsentView::UpdateUiText() {
  const std::u16string title = GetTitle(intent_type_, intent_text_);
  title_->SetText(title);
  GetViewAccessibility().SetName(title);
}

BEGIN_METADATA(UserConsentView)
END_METADATA

}  // namespace quick_answers
