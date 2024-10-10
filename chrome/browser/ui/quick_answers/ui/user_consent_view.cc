// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/user_consent_view.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"
#include "chrome/browser/ui/quick_answers/ui/typography.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
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
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
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
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace quick_answers {

namespace {

// Main view (or common) specs.
constexpr int kLineHeightDip = 20;
constexpr int kContentSpacingDip = 8;
constexpr auto kMainViewInsets = gfx::Insets::TLBR(16, 12, 16, 16);
constexpr auto kContentInsets = gfx::Insets::TLBR(0, 12, 0, 0);
constexpr auto kContentInsetsRefresh = gfx::Insets::TLBR(0, 16, 0, 0);

// Icon.
constexpr int kGoogleIconSizeDip = 16;
constexpr int kIntentIconSizeDip = 20;
constexpr int kIconBackgroundCornerRadiusDip = 12;
constexpr gfx::Insets kIntentIconInsets = gfx::Insets(8);

// Label.
constexpr int kTitleFontSizeDelta = 2;
constexpr int kDescFontSizeDelta = 1;
constexpr gfx::Insets kLabelMargin =
    gfx::Insets::TLBR(0, 0, kContentSpacingDip, 0);

// Buttons common.
constexpr int kButtonSpacingDip = 8;
constexpr auto kButtonBarInsets = gfx::Insets::TLBR(8, 0, 0, 0);
constexpr auto kButtonInsets = gfx::Insets::TLBR(6, 16, 6, 16);
constexpr int kButtonFontSizeDelta = 1;

// Compact buttons layout.
constexpr int kCompactButtonLayoutThreshold = 200;
constexpr auto kCompactButtonInsets = gfx::Insets::TLBR(6, 12, 6, 12);
constexpr int kCompactButtonFontSizeDelta = 0;

std::u16string ToUiString(IntentType intent_type) {
  switch (intent_type) {
    case IntentType::kUnit:
      return l10n_util::GetStringUTF16(
          IDS_QUICK_ANSWERS_UNIT_CONVERSION_INTENT);
    case IntentType::kDictionary:
      return l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_DEFINITION_INTENT);
    case IntentType::kTranslation:
      return l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_TRANSLATION_INTENT);
    case IntentType::kUnknown:
      return std::u16string();
  }

  CHECK(false) << "Invalid intent type enum class provided";
}

int GetActualLabelWidth(int anchor_view_width) {
  return anchor_view_width - kMainViewInsets.width() - kContentInsets.width() -
         kGoogleIconSizeDip;
}

bool ShouldUseCompactButtonLayout(int anchor_view_width) {
  return GetActualLabelWidth(anchor_view_width) < kCompactButtonLayoutThreshold;
}

views::Builder<views::Label> GetConfiguredLabelBuilder(
    bool use_refreshed_design,
    bool is_first_line) {
  if (use_refreshed_design) {
    return views::Builder<views::Label>()
        .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
        .SetLineHeight(is_first_line ? GetFirstLineHeight(Design::kRefresh)
                                     : GetSecondLineHeight(Design::kRefresh))
        .SetFontList(is_first_line ? GetFirstLineFontList(Design::kRefresh)
                                   : GetSecondLineFontList(Design::kRefresh));
  }

  return views::Builder<views::Label>()
      // TODO(b/340628664): This is from old code. Consider if we can remove
      // AutoColorReadabilityEnabled=false.
      .SetAutoColorReadabilityEnabled(false)
      .SetLineHeight(kLineHeightDip)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetFontList(views::Label::GetDefaultFontList().DeriveWithSizeDelta(
          is_first_line ? kTitleFontSizeDelta : kDescFontSizeDelta));
}

// `views::LabelButton` with custom line-height, color and font-list for the
// underlying label. Extend `views::MdTextButton` to access `label()`, which is
// a protected method.
class CustomizedLabelButton : public views::MdTextButton {
  METADATA_HEADER(CustomizedLabelButton, views::MdTextButton)

 public:
  explicit CustomizedLabelButton(bool is_compact) {
    SetCustomPadding(is_compact ? kCompactButtonInsets : kButtonInsets);
    label()->SetLineHeight(kLineHeightDip);
    label()->SetFontList(
        views::Label::GetDefaultFontList()
            .DeriveWithSizeDelta(is_compact ? kCompactButtonFontSizeDelta
                                            : kButtonFontSizeDelta)
            .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  }

  // Disallow copy and assign.
  CustomizedLabelButton(const CustomizedLabelButton&) = delete;
  CustomizedLabelButton& operator=(const CustomizedLabelButton&) = delete;

  ~CustomizedLabelButton() override = default;
};

BEGIN_METADATA(CustomizedLabelButton)
END_METADATA

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

  CHECK(false) << "An invalid IntentType enum class value is provided";
}

std::u16string GetTitle(IntentType intent_type,
                        const std::u16string& intent_text) {
  if (intent_type == IntentType::kUnknown || intent_text.empty()) {
    return l10n_util::GetStringUTF16(
        IDS_QUICK_ANSWERS_USER_NOTICE_VIEW_TITLE_TEXT);
  }

  // TODO(b/340628664): stop building a UI string with string concatenation as
  // it can cause complications in UI translations.
  return l10n_util::GetStringFUTF16(
      IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_TITLE_TEXT_WITH_INTENT,
      ToUiString(intent_type), intent_text);
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

  CHECK(false) << "An invalid IntentType enum class value is provided";
}

std::u16string GetTitleForRefreshedUi(IntentType intent_type,
                                      const std::u16string& intent_text) {
  std::optional<int> message_id = GetTitleMessageIdFor(intent_type);
  if (!message_id.has_value() || intent_text.empty()) {
    // This is used only from Linux-ChromeOS, i.e., non-prod environment.
    return l10n_util::GetStringUTF16(
        IDS_QUICK_ANSWERS_USER_NOTICE_VIEW_TITLE_TEXT);
  }

  return l10n_util::GetStringFUTF16(message_id.value(), intent_text);
}

views::Builder<views::ImageView> GetGoogleIcon() {
  return views::Builder<views::ImageView>()
      .SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
          (kLineHeightDip - kGoogleIconSizeDip) / 2, 0, 0, 0)))
      .SetImage(ui::ImageModel::FromVectorIcon(vector_icons::kGoogleColorIcon,
                                               gfx::kPlaceholderColor,
                                               kGoogleIconSizeDip));
}

views::Builder<views::ImageView> GetIconFor(IntentType intent_type) {
  return views::Builder<views::ImageView>().SetImage(
      ui::ImageModel::FromVectorIcon(
          GetResultTypeIcon(ToResultType(intent_type)), ui::kColorSysOnSurface,
          kIntentIconSizeDip));
}

views::Builder<views::MdTextButton> GetButtonBuilder(bool use_refreshed_design,
                                                     int context_menu_width) {
  if (use_refreshed_design) {
    return views::Builder<views::MdTextButton>();
  }

  return views::Builder<views::MdTextButton>(
      std::make_unique<CustomizedLabelButton>(
          ShouldUseCompactButtonLayout(context_menu_width)));
}

}  // namespace

// UserConsentView
// -------------------------------------------------------------

UserConsentView::UserConsentView(
    bool use_refreshed_design,
    chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller)
    : chromeos::ReadWriteCardsView(read_write_cards_ui_controller),
      focus_search_(this,
                    base::BindRepeating(&UserConsentView::GetFocusableViews,
                                        base::Unretained(this))),
      use_refreshed_design_(use_refreshed_design) {
  SetUseDefaultFillLayout(true);

  views::FlexLayoutView* content;
  views::FlexLayoutView* buttons_container;

  // This is to avoid 80 char limit lint errors caused by long message ids and
  // indents.
  constexpr int kDescriptionMessageId =
      IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_DESC_TEXT;
  constexpr int kDescriptionRefreshedMessageId =
      IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_DESCRIPTION_TEXT;
  constexpr int kNoThanksButtonMessageId =
      IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_NO_THANKS_BUTTON;
  constexpr int kAllowButtonMessageId =
      IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_ALLOW_BUTTON;
  constexpr int kTryItButtonMessageId =
      IDS_QUICK_ANSWERS_USER_CONSENT_VIEW_TRY_IT_BUTTON;

  AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetInteriorMargin(kMainViewInsets)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
          .AddChild(views::Builder<views::FlexLayoutView>()
                        .SetBackground(views::CreateThemedRoundedRectBackground(
                            ui::kColorSysPrimaryContainer,
                            kIconBackgroundCornerRadiusDip))
                        .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
                        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
                        .SetInteriorMargin(kIntentIconInsets)
                        .SetVisible(use_refreshed_design_)
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
          .AddChild(GetGoogleIcon().SetVisible(!use_refreshed_design_))
          .AddChild(
              views::Builder<views::FlexLayoutView>()
                  .CopyAddressTo(&content)
                  .SetOrientation(views::LayoutOrientation::kVertical)
                  .SetIgnoreDefaultMainAxisMargins(true)
                  .SetInteriorMargin(use_refreshed_design_
                                         ? kContentInsetsRefresh
                                         : kContentInsets)
                  .SetCollapseMargins(true)
                  .AddChild(
                      GetConfiguredLabelBuilder(use_refreshed_design_,
                                                /*is_first_line=*/true)
                          .CopyAddressTo(&title_)
                          .SetProperty(views::kMarginsKey, kLabelMargin)
                          .SetProperty(
                              views::kFlexBehaviorKey,
                              views::FlexSpecification(
                                  views::MinimumFlexSizeRule::kScaleToMinimum,
                                  views::MaximumFlexSizeRule::kPreferred)))
                  .AddChild(
                      GetConfiguredLabelBuilder(use_refreshed_design_,
                                                /*is_first_line=*/false)
                          .CopyAddressTo(&description_)
                          .SetText(l10n_util::GetStringUTF16(
                              use_refreshed_design_
                                  ? kDescriptionRefreshedMessageId
                                  : kDescriptionMessageId))
                          .SetMultiLine(true)
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
                              GetButtonBuilder(use_refreshed_design_,
                                               context_menu_bounds().width())
                                  .CopyAddressTo(&no_thanks_button_)
                                  .SetText(l10n_util::GetStringUTF16(
                                      kNoThanksButtonMessageId))
                                  .SetCallback(base::BindRepeating(
                                      &QuickAnswersUiController::
                                          OnUserConsentResult,
                                      controller_, false))
                                  .SetStyle(use_refreshed_design_
                                                ? ui::ButtonStyle::kText
                                                : ui::ButtonStyle::kDefault)
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
                              GetButtonBuilder(use_refreshed_design_,
                                               context_menu_bounds().width())
                                  .CopyAddressTo(&allow_button_)
                                  .SetText(l10n_util::GetStringUTF16(
                                      use_refreshed_design_
                                          ? kTryItButtonMessageId
                                          : kAllowButtonMessageId))
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
      l10n_util::GetStringUTF16(use_refreshed_design_
                                    ? kDescriptionRefreshedMessageId
                                    : kDescriptionMessageId)));
  // Read out user-consent text if screen-reader is active.
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      IDS_QUICK_ANSWERS_USER_NOTICE_VIEW_A11Y_INFO_ALERT_TEXT));

  UpdateIcon();
  UpdateUiText();

  // Focus should cycle to each of the buttons the view contains and back to it.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
  views::FocusRing::Install(this);
}

UserConsentView::~UserConsentView() = default;

void UserConsentView::OnFocus() {
  // Unless screen-reader mode is enabled, transfer the focus to an actionable
  // button, otherwise retain to read out its contents.
  if (QuickAnswersState::Get()->spoken_feedback_enabled()) {
    no_thanks_button_->RequestFocus();
  }
}

void UserConsentView::OnThemeChanged() {
  views::View::OnThemeChanged();

  // TODO(b/340628664): Delete `UserConsentView::OnThemeChanged`. Let
  // `views::Label`, etc handle those color changes.
  SetBackground(views::CreateSolidBackground(
      GetColorProvider()->GetColor(ui::kColorPrimaryBackground)));
  title_->SetEnabledColor(
      GetColorProvider()->GetColor(ui::kColorLabelForeground));
  description_->SetEnabledColor(
      GetColorProvider()->GetColor(ui::kColorLabelForegroundSecondary));
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
  // Intent specific icons are used only in a refreshed design.
  if (!use_refreshed_design_) {
    return;
  }

  dictionary_intent_icon_->SetVisible(intent_type_ == IntentType::kDictionary);
  translation_intent_icon_->SetVisible(intent_type_ ==
                                       IntentType::kTranslation);
  unit_intent_icon_->SetVisible(intent_type_ == IntentType::kUnit);
  unknown_intent_icon_->SetVisible(intent_type_ == IntentType::kUnknown);
}

void UserConsentView::UpdateUiText() {
  title_->SetText(use_refreshed_design_
                      ? GetTitleForRefreshedUi(intent_type_, intent_text_)
                      : GetTitle(intent_type_, intent_text_));

  GetViewAccessibility().SetName(
      use_refreshed_design_ ? GetTitleForRefreshedUi(intent_type_, intent_text_)
                            : GetTitle(intent_type_, intent_text_));
}

BEGIN_METADATA(UserConsentView)
END_METADATA

}  // namespace quick_answers
