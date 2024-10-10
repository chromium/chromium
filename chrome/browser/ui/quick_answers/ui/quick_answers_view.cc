// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"

#include <string>
#include <string_view>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/loading_view.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_stage_button.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_text_label.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"
#include "chrome/browser/ui/quick_answers/ui/result_view.h"
#include "chrome/browser/ui/quick_answers/ui/retry_view.h"
#include "chrome/browser/ui/quick_answers/ui/typography.h"
#include "chrome/browser/ui/views/editor_menu/utils/focus_search.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chromeos/components/magic_boost/public/cpp/views/experiment_badge.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace quick_answers {
namespace {

using quick_answers::PhoneticsInfo;
using quick_answers::QuickAnswer;
using quick_answers::QuickAnswerResultText;
using quick_answers::QuickAnswerText;
using quick_answers::QuickAnswerUiElement;
using quick_answers::QuickAnswerUiElementType;
using quick_answers::ResultType;
using views::Button;
using views::Label;
using views::View;

constexpr auto kMainViewInsets = gfx::Insets::TLBR(12, 8, 12, 16);
constexpr auto kContentViewInsets = gfx::Insets::TLBR(0, 8, 0, 0);

// Google icon.
constexpr int kIconSizeDip = 16;

// Spacing between lines in the main view.
constexpr int kLineSpacingDip = 4;
constexpr int kDefaultLineHeightDip = 20;

// Buttons view.
constexpr int kButtonsViewMarginDip = 4;
constexpr int kButtonsSpacingDip = 4;
constexpr int kDogfoodButtonSizeDip = 20;
constexpr int kSettingsButtonSizeDip = 14;
constexpr int kSettingsButtonBorderDip = 3;

const gfx::Insets GetMainViewInsets(Design design) {
  switch (design) {
    case Design::kCurrent:
      return kMainViewInsets;
    case Design::kRefresh:
    case Design::kMagicBoost:
      return gfx::Insets::TLBR(12, 16, 16, 16);
  }

  CHECK(false) << "Invalid design enum value provided";
}

const gfx::Insets GetIconInsets(Design design) {
  switch (design) {
    case Design::kCurrent:
      return gfx::Insets(views::LayoutProvider::Get()->GetInsetsMetric(
          views::InsetsMetric::INSETS_ICON_BUTTON));
    case Design::kRefresh:
    case Design::kMagicBoost:
      return gfx::Insets::TLBR(2, 0, 0, 0);
  }

  CHECK(false) << "Invalid design enum value provided";
}

const gfx::Insets GetButtonsViewInsets(Design design) {
  switch (design) {
    case Design::kCurrent:
      return gfx::Insets(kButtonsViewMarginDip);
    case Design::kRefresh:
    case Design::kMagicBoost:
      // Buttons view is rendered as a layer on top of main view. For `kRefresh`
      // and `kMagicBoost`, they share the same insets.
      return GetMainViewInsets(design);
  }

  CHECK(false) << "Invalid design enum value provided";
}

const gfx::VectorIcon& GetVectorIcon(std::optional<Intent> intent) {
  if (!intent) {
    return omnibox::kAnswerDefaultIcon;
  }

  switch (intent.value()) {
    case Intent::kDefinition:
      return chromeos::kDictionaryIcon;
    case Intent::kTranslation:
      return omnibox::kAnswerTranslationIcon;
    case Intent::kUnitConversion:
      return omnibox::kAnswerCalculatorIcon;
  }

  CHECK(false) << "Invalid intent enum value specified";
}

ui::ImageModel GetIcon(Design design, std::optional<Intent> intent) {
  switch (design) {
    case Design::kCurrent:
      return ui::ImageModel::FromVectorIcon(
          vector_icons::kGoogleColorIcon, gfx::kPlaceholderColor, kIconSizeDip);
    case Design::kRefresh:
      return ui::ImageModel::FromVectorIcon(
          GetVectorIcon(intent), ui::kColorSysOnSurface, kIconSizeDip);
    case Design::kMagicBoost:
      return ui::ImageModel::FromVectorIcon(chromeos::kInfoSparkIcon,
                                            ui::ColorIds::kColorSysOnSurface,
                                            kIconSizeDip);
  }

  CHECK(false) << "Invalid design enum value specified";
}

void SetResultTo(ResultView* result_view, DefinitionResult* definition_result) {
  if (definition_result->phonetics_info.text.empty()) {
    result_view->SetFirstLineText(base::UTF8ToUTF16(definition_result->word));
  } else {
    result_view->SetFirstLineText(base::UTF8ToUTF16(BuildDefinitionTitleText(
        definition_result->word, definition_result->phonetics_info.text)));
  }

  if (definition_result->phonetics_info.PhoneticsInfoAvailable()) {
    result_view->SetPhoneticsInfo(definition_result->phonetics_info);
  }

  result_view->SetSecondLineText(
      base::UTF8ToUTF16(definition_result->sense.definition));
}

void SetResultTo(ResultView* result_view,
                 TranslationResult* translation_result,
                 Design design) {
  result_view->SetFirstLineText(
      base::UTF8ToUTF16(translation_result->text_to_translate));

  if (design != Design::kCurrent) {
    std::u16string display_name_locale =
        l10n_util::GetDisplayNameForLocaleWithoutCountry(
            translation_result->source_locale,
            g_browser_process->GetApplicationLocale(), /*is_for_ui=*/true);
    if (!display_name_locale.empty()) {
      result_view->SetFirstLineSubText(display_name_locale);
    }
  }

  result_view->SetSecondLineText(
      base::UTF8ToUTF16(translation_result->translated_text));
}

void SetResultTo(ResultView* result_view,
                 UnitConversionResult* unit_conversion_result) {
  result_view->SetFirstLineText(
      base::UTF8ToUTF16(unit_conversion_result->source_text));
  result_view->SetSecondLineText(
      base::UTF8ToUTF16(unit_conversion_result->result_text));
}

void SetNoResult(ResultView* result_view, std::string_view title) {
  result_view->SetFirstLineText(base::UTF8ToUTF16(title));
  result_view->SetSecondLineText(
      l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_VIEW_NO_RESULT_V2));
}

std::u16string GetIntentName(std::optional<Intent> intent) {
  if (!intent) {
    // The original intent name is kept if the backend finds out that it cannot
    // find an intent in a query. It means that `std::nullopt` case is shown
    // only for Linux-ChromeOS.
    return std::u16string();
  }

  switch (intent.value()) {
    case Intent::kDefinition:
      return l10n_util::GetStringUTF16(
          IDS_QUICK_ANSWERS_RESULT_HEADER_INTENT_DEFINITION);
    case Intent::kTranslation:
      return l10n_util::GetStringUTF16(
          IDS_QUICK_ANSWERS_RESULT_HEADER_INTENT_TRANSLATION);
    case Intent::kUnitConversion:
      return l10n_util::GetStringUTF16(
          IDS_QUICK_ANSWERS_RESULT_HEADER_INTENT_UNIT_CONVERSION);
  }

  CHECK(false) << "Invalid intent enum value specified";
}

// TODO(b/340629098): A temporary solution until buttons view is merged into
// headers. See another comment for buttons view in
// `QuickAnswersView::QuickAnswersView` about details.
int GetButtonsViewOcclusion(Design design) {
  gfx::Insets insets_icon_button =
      views::LayoutProvider::Get()->GetInsetsMetric(
          views::InsetsMetric::INSETS_ICON_BUTTON);
  return insets_icon_button.left() + kIconSizeDip + insets_icon_button.right() +
         GetButtonsViewInsets(design).right();
}

views::Builder<views::Label> GetRefreshUiHeader() {
  int line_height = GetCrosAnnotation1LineHeight();
  int vertical_padding = std::max(0, (20 - line_height) / 2);

  return views::Builder<views::Label>()
      .SetFontList(GetCrosAnnotation1FontList().DeriveWithWeight(
          gfx::Font::Weight::MEDIUM))
      .SetLineHeight(line_height)
      .SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(
              vertical_padding, 0,
              vertical_padding +
                  views::LayoutProvider::Get()->GetDistanceMetric(
                      views::DistanceMetric::DISTANCE_RELATED_CONTROL_VERTICAL),
              GetButtonsViewOcclusion(Design::kRefresh)))
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred));
}

views::Builder<views::BoxLayoutView> GetMagicBoostHeader() {
  int line_height = GetCrosAnnotation1LineHeight();
  int vertical_padding = std::max(0, (20 - line_height) / 2);

  return views::Builder<views::BoxLayoutView>()
      .SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(
              0, 0,
              views::LayoutProvider::Get()->GetDistanceMetric(
                  views::DistanceMetric::DISTANCE_RELATED_CONTROL_VERTICAL),
              GetButtonsViewOcclusion(Design::kMagicBoost)))
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kPreferred))
      .SetBetweenChildSpacing(views::LayoutProvider::Get()->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL))
      .AddChild(
          views::Builder<views::Label>()
              .SetText(l10n_util::GetStringUTF16(IDS_ASH_MAHI_MENU_TITLE))
              .SetLineHeight(line_height)
              .SetProperty(views::kMarginsKey,
                           gfx::Insets::VH(vertical_padding, 0))
              .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
              .SetFontList(GetCrosAnnotation1FontList().DeriveWithWeight(
                  gfx::Font::Weight::MEDIUM)))
      .AddChild(views::Builder<chromeos::ExperimentBadge>());
}

std::string GetResultA11yDescription(ResultView* result_view,
                                     std::optional<Intent> intent,
                                     Design design) {
  bool include_intent = design == Design::kRefresh && intent;
  bool include_second_line_text = !result_view->GetFirstLineSubText().empty();

  if (include_intent) {
    if (include_second_line_text) {
      return l10n_util::GetStringFUTF8(
          IDS_QUICK_ANSWERS_VIEW_A11Y_INFO_DESCRIPTION_WITH_INTENT_AND_SUBTEXT_TEMPLATE,
          GetIntentName(intent), result_view->GetFirstLineText(),
          result_view->GetFirstLineSubText(), result_view->GetSecondLineText());
    }

    return l10n_util::GetStringFUTF8(
        IDS_QUICK_ANSWERS_VIEW_A11Y_INFO_DESCRIPTION_WITH_INTENT_TEMPLATE,
        GetIntentName(intent), result_view->GetFirstLineText(),
        result_view->GetSecondLineText());
  }

  if (include_second_line_text) {
    return l10n_util::GetStringFUTF8(
        IDS_QUICK_ANSWERS_VIEW_A11Y_INFO_DESCRIPTION_WITH_SUBTEXT_TEMPLATE,
        result_view->GetFirstLineText(), result_view->GetFirstLineSubText(),
        result_view->GetSecondLineText());
  }

  return l10n_util::GetStringFUTF8(
      IDS_QUICK_ANSWERS_VIEW_A11Y_INFO_DESCRIPTION_TEMPLATE,
      result_view->GetFirstLineText(), result_view->GetSecondLineText());
}

}  // namespace

// QuickAnswersView -----------------------------------------------------------

QuickAnswersView::QuickAnswersView(
    const QuickAnswersView::Params& params,
    base::WeakPtr<QuickAnswersUiController> controller)
    : chromeos::ReadWriteCardsView(controller->GetReadWriteCardsUiController()),
      controller_(std::move(controller)),
      title_(params.title),
      design_(params.design),
      is_internal_(params.is_internal),
      focus_search_(std::make_unique<chromeos::editor_menu::FocusSearch>(
          this,
          base::BindRepeating(&QuickAnswersView::GetFocusableViews,
                              base::Unretained(this)))) {
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorPrimaryBackground));
  SetUseDefaultFillLayout(true);

  std::unique_ptr<views::FlexLayout> main_view_layout =
      std::make_unique<views::FlexLayout>();
  main_view_layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetInteriorMargin(GetMainViewInsets(design_))
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  AddChildView(
      views::Builder<QuickAnswersStageButton>()
          .CopyAddressTo(&quick_answers_stage_button_)
          .SetCallback(base::BindRepeating(
              &QuickAnswersView::SendQuickAnswersQuery, base::Unretained(this)))
          .SetLayoutManager(std::move(main_view_layout))
          .AddChild(views::Builder<views::ImageView>().CopyAddressTo(&icon_))
          .AddChild(
              views::Builder<views::FlexLayoutView>()
                  .SetInteriorMargin(kContentViewInsets)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width=*/true))
                  .SetOrientation(views::LayoutOrientation::kVertical)
                  .AddChild(GetRefreshUiHeader()
                                .SetVisible(design_ == Design::kRefresh)
                                .CopyAddressTo(&refreshed_ui_header_))
                  .AddChild(GetMagicBoostHeader().SetVisible(
                      design_ == Design::kMagicBoost))
                  .AddChild(
                      views::Builder<LoadingView>()
                          .CopyAddressTo(&loading_view_)
                          .SetFirstLineText(base::UTF8ToUTF16(title_))
                          .SetDesign(design_)
                          .SetProperty(
                              views::kFlexBehaviorKey,
                              views::FlexSpecification(
                                  views::MinimumFlexSizeRule::kPreferred,
                                  views::MaximumFlexSizeRule::kPreferred)))
                  .AddChild(
                      views::Builder<RetryView>()
                          .CopyAddressTo(&retry_view_)
                          .SetVisible(false)
                          .SetFirstLineText(base::UTF8ToUTF16(title_))
                          .SetRetryButtonCallback(base::BindRepeating(
                              &QuickAnswersUiController::OnRetryLabelPressed,
                              controller_))
                          .SetDesign(design_)
                          .SetProperty(
                              views::kFlexBehaviorKey,
                              views::FlexSpecification(
                                  views::MinimumFlexSizeRule::kPreferred,
                                  views::MaximumFlexSizeRule::kPreferred)))
                  .AddChild(views::Builder<ResultView>()
                                .CopyAddressTo(&result_view_)
                                .SetVisible(false)
                                .SetProperty(
                                    views::kFlexBehaviorKey,
                                    views::FlexSpecification(
                                        views::MinimumFlexSizeRule::kPreferred,
                                        views::MaximumFlexSizeRule::kPreferred,
                                        /*adjust_height_for_width=*/true))
                                .SetDesign(design_)
                                .SetGenerateTtsCallback(base::BindRepeating(
                                    &QuickAnswersView::GenerateTts,
                                    base::Unretained(this)))))
          .Build());

  // TODO(b/340629098): For `kRefresh` and `kMagicBoost`, buttons are in the
  // same row of a header. Move buttons to the row.
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
          .SetInsideBorderInsets(GetButtonsViewInsets(design_))
          .SetBetweenChildSpacing(kButtonsSpacingDip)
          .AddChild(
              views::Builder<views::ImageButton>()
                  .CopyAddressTo(&dogfood_button_)
                  .SetVisible(is_internal_)
                  .SetCallback(base::BindRepeating(
                      &QuickAnswersUiController::OnReportQueryButtonPressed,
                      controller_))
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_QUICK_ANSWERS_DOGFOOD_FEEDBACK_BUTTON_TOOLTIP_TEXT))
                  .SetImageModel(
                      views::Button::STATE_NORMAL,
                      ui::ImageModel::FromVectorIcon(
                          vector_icons::kDogfoodIcon,
                          design_ == Design::kCurrent ? ui::kColorIconSecondary
                                                      : ui::kColorSysSecondary,
                          kDogfoodButtonSizeDip)))
          .AddChild(
              views::Builder<views::ImageButton>()
                  .CopyAddressTo(&settings_button_)
                  .SetCallback(base::BindRepeating(
                      &QuickAnswersUiController::OnSettingsButtonPressed,
                      controller_))
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_RICH_ANSWERS_VIEW_SETTINGS_BUTTON_A11Y_NAME_TEXT))
                  .SetImageModel(
                      views::Button::ButtonState::STATE_NORMAL,
                      ui::ImageModel::FromVectorIcon(
                          vector_icons::kSettingsOutlineIcon,
                          design_ == Design::kCurrent ? ui::kColorIconSecondary
                                                      : ui::kColorSysSecondary,
                          kSettingsButtonSizeDip))
                  .SetProperty(views::kMarginsKey,
                               gfx::Insets(kSettingsButtonBorderDip)))
          .Build());

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();

  UpdateViewAccessibility();
  UpdateIcon();
  UpdateUiText();
}

QuickAnswersView::~QuickAnswersView() = default;

void QuickAnswersView::RequestFocus() {
  // When the Quick Answers view is focused, we actually want `main_view_`
  // to have the focus for highlight and selection purposes.
  quick_answers_stage_button_->RequestFocus();
}

bool QuickAnswersView::HasFocus() const {
  // When the Quick Answers view is focused, `main_view_` should have
  // the focus.
  return quick_answers_stage_button_->HasFocus();
}

void QuickAnswersView::OnFocus() {
  View* wants_focus = focus_search_->FindNextFocusableView(
      nullptr, views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kCheckStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kSkipAnchoredDialog, nullptr,
      nullptr);
  if (wants_focus != this) {
    wants_focus->RequestFocus();
  } else {
    NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
  }
}

views::FocusTraversable* QuickAnswersView::GetPaneFocusTraversable() {
  return focus_search_.get();
}

void QuickAnswersView::UpdateUiText() {
  if (design_ != Design::kRefresh) {
    return;
  }

  refreshed_ui_header_->SetText(GetIntentName(intent_));
}

void QuickAnswersView::UpdateViewAccessibility() {
  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetDefaultActionVerb(
      ax::mojom::DefaultActionVerb::kNone);

  // The view itself is not focused for retry-mode, so should not be announced
  // by the screen reader.
  if (retry_view_->GetVisible()) {
    GetViewAccessibility().SetName(
        std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
    return;
  }

  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF8(IDS_QUICK_ANSWERS_VIEW_A11Y_NAME_TEXT));

  if (result_view_->GetVisible()) {
    GetViewAccessibility().SetDefaultActionVerb(
        ax::mojom::DefaultActionVerb::kClick);
    GetViewAccessibility().SetDescription(
        GetResultA11yDescription(result_view_, intent_, design_));
  }
}

// A11y actions are sent to `QuickAnswersView`. Route default action as a click.
bool QuickAnswersView::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  if (action_data.action == ax::mojom::Action::kDoDefault) {
    quick_answers_stage_button_->button_controller()->NotifyClick();
    return true;
  }

  return views::View::HandleAccessibleAction(action_data);
}

void QuickAnswersView::UpdateIcon() {
  icon_->SetProperty(views::kMarginsKey, GetIconInsets(design_));
  icon_->SetImage(GetIcon(design_, intent_));
}

gfx::Size QuickAnswersView::GetMaximumSize() const {
  // TODO(b/340629098): update this. Different line heights are used for
  // `kRefresh` and `kMagicBoost`.
  int maximum_height =
      GetMainViewInsets(design_).height() + kContentViewInsets.height() +
      kDefaultLineHeightDip + kLineSpacingDip +
      quick_answers::ResultView::kMaxLines * kDefaultLineHeightDip;

  // The maximum height will be used in calculating the position of the widget
  // in `ReadWriteCardsUiController`. We need to reserve space at
  // the top since the view might expand for two-line answers.
  // Note that the width will not be used in the calculation.
  return gfx::Size(0, maximum_height);
}

void QuickAnswersView::UpdateBoundsForQuickAnswers() {
  // TODO(b/331271987): Remove this and the interface.
}

void QuickAnswersView::SendQuickAnswersQuery() {
  if (controller_) {
    controller_->OnQuickAnswersViewPressed();
  }
}

void QuickAnswersView::SwitchTo(views::View* view) {
  CHECK(view == loading_view_ || view == retry_view_ || view == result_view_);

  loading_view_->SetVisible(view == loading_view_);
  retry_view_->SetVisible(view == retry_view_);
  result_view_->SetVisible(view == result_view_);

  UpdateViewAccessibility();
}

void QuickAnswersView::ShowRetryView() {
  SwitchTo(retry_view_);
}

bool QuickAnswersView::ShouldAddPhoneticsAudioButton(ResultType result_type,
                                                     GURL phonetics_audio,
                                                     bool tts_audio_enabled) {
  if (result_type != ResultType::kDefinitionResult) {
    return false;
  }

  return !phonetics_audio.is_empty() || tts_audio_enabled;
}

void QuickAnswersView::SetMockGenerateTtsCallbackForTesting(
    QuickAnswersView::MockGenerateTtsCallback mock_generate_tts_callback) {
  CHECK_IS_TEST();
  CHECK(!mock_generate_tts_callback.is_null());
  CHECK(mock_generate_tts_callback_.is_null());
  mock_generate_tts_callback_ = mock_generate_tts_callback;
}

bool QuickAnswersView::HasFocusInside() {
  views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    // `focus_manager` can be nullptr only in a pixel test.
    CHECK_IS_TEST();
    return false;
  }

  return Contains(focus_manager->GetFocusedView());
}

void QuickAnswersView::SetIntent(Intent intent) {
  intent_ = intent;

  UpdateUiText();
  UpdateIcon();
}

std::optional<Intent> QuickAnswersView::GetIntent() const {
  return intent_;
}

void QuickAnswersView::SetResult(const StructuredResult& structured_result) {
  // Check if the view (or any of its children) had focus before resetting the
  // view, so it can be restored for the updated view.
  bool pane_already_had_focus = HasFocusInside();

  SwitchTo(result_view_);

  switch (structured_result.GetResultType()) {
    case ResultType::kDefinitionResult:
      SetIntent(Intent::kDefinition);
      SetResultTo(result_view_, structured_result.definition_result.get());
      break;
    case ResultType::kTranslationResult:
      SetIntent(Intent::kTranslation);
      SetResultTo(result_view_, structured_result.translation_result.get(),
                  design_);
      break;
    case ResultType::kUnitConversionResult:
      SetIntent(Intent::kUnitConversion);
      SetResultTo(result_view_, structured_result.unit_conversion_result.get());
      break;
    case ResultType::kNoResult:
      // Do not set intent back to unknown (i.e., `kNoResult`). See the comment
      // of `SetIntent` in the header file.
      SetNoResult(result_view_, title_);
      break;
  }

  // Restore focus if the view had one prior to updating the answer.
  if (pane_already_had_focus) {
    RequestFocus();
  } else {
    // Announce that a Quick Answer is available.
    GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_VIEW_A11Y_INFO_ALERT_TEXT));
  }

  UpdateViewAccessibility();
}

// TODO(b/335701090): Move this out from QuickAnswersView to the controller.
void QuickAnswersView::GenerateTts(const PhoneticsInfo& phonetics_info) {
  if (!mock_generate_tts_callback_.is_null()) {
    CHECK_IS_TEST();
    mock_generate_tts_callback_.Run(phonetics_info);
    return;
  }

  if (!phonetics_audio_web_view_) {
    // Setup an invisible WebView to play phonetics audio.
    std::unique_ptr<views::WebView> web_view = std::make_unique<views::WebView>(
        ProfileManager::GetActiveUserProfile());
    web_view->SetVisible(false);
    phonetics_audio_web_view_.SetView(AddChildView(std::move(web_view)));
  }

  views::WebView* phonetics_audio_web_view =
      views::AsViewClass<views::WebView>(phonetics_audio_web_view_.view());
  CHECK(phonetics_audio_web_view);

  // Use the phonetics audio URL if provided.
  if (!phonetics_info.phonetics_audio.is_empty()) {
    phonetics_audio_web_view->LoadInitialURL(phonetics_info.phonetics_audio);
    return;
  }

  GenerateTTSAudio(phonetics_audio_web_view->GetBrowserContext(),
                   phonetics_info.query_text, phonetics_info.locale);
}

std::vector<views::View*> QuickAnswersView::GetFocusableViews() {
  std::vector<views::View*> focusable_views;
  // The main view does not gain focus for retry-view and transfers it to the
  // retry-label, and so is not included when this is the case.
  if (!retry_view_->GetVisible()) {
    focusable_views.push_back(quick_answers_stage_button_);
  }

  if (dogfood_button_ && dogfood_button_->GetVisible()) {
    focusable_views.push_back(dogfood_button_);
  }

  if (settings_button_ && settings_button_->GetVisible()) {
    focusable_views.push_back(settings_button_);
  }

  if (retry_view_->GetVisible()) {
    focusable_views.push_back(retry_view_->retry_label_button());
  }

  if (result_view_->GetVisible() &&
      result_view_->phonetics_audio_button()->GetVisible()) {
    focusable_views.push_back(result_view_->phonetics_audio_button());
  }

  return focusable_views;
}

BEGIN_METADATA(QuickAnswersView)
ADD_READONLY_PROPERTY_METADATA(std::optional<Intent>, Intent)
END_METADATA

}  // namespace quick_answers
