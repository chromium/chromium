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
#include "chrome/browser/ui/views/editor_menu/utils/focus_search.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
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
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
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

constexpr auto kMainViewInsets = gfx::Insets::VH(4, 0);
constexpr auto kContentViewInsets = gfx::Insets::TLBR(8, 0, 8, 16);

// Google icon.
constexpr int kIconSizeDip = 16;
constexpr auto kIconInsets = gfx::Insets::TLBR(10, 10, 0, 10);

// Spacing between lines in the main view.
constexpr int kLineSpacingDip = 4;
constexpr int kDefaultLineHeightDip = 20;

// Buttons view.
constexpr int kButtonsViewMarginDip = 4;
constexpr int kButtonsSpacingDip = 4;
constexpr int kDogfoodButtonSizeDip = 20;
constexpr int kSettingsButtonSizeDip = 14;
constexpr int kSettingsButtonBorderDip = 3;

constexpr int kMaximumHeight =
    kMainViewInsets.height() + kContentViewInsets.height() +
    kDefaultLineHeightDip + kLineSpacingDip +
    quick_answers::ResultView::kMaxLines * kDefaultLineHeightDip;

const gfx::VectorIcon& GetVectorIcon(QuickAnswersView::Intent intent) {
  switch (intent) {
    case QuickAnswersView::Intent::kDefinition:
      return omnibox::kAnswerDictionaryIcon;
    case QuickAnswersView::Intent::kTranslation:
      return omnibox::kAnswerTranslationIcon;
    case QuickAnswersView::Intent::kUnitConversion:
      return omnibox::kAnswerCalculatorIcon;
    case QuickAnswersView::Intent::kUndefined:
      CHECK(false) << "Invalid intent value specified";
      return omnibox::kAnswerDefaultIcon;
  }

  CHECK(false) << "Invalid intent enum value specified";
}

ui::ImageModel GetIcon(QuickAnswersView::Design design,
                       QuickAnswersView::Intent intent) {
  switch (design) {
    case QuickAnswersView::Design::kCurrent:
      return ui::ImageModel::FromVectorIcon(
          vector_icons::kGoogleColorIcon, gfx::kPlaceholderColor, kIconSizeDip);
    case QuickAnswersView::Design::kRefresh:
      return ui::ImageModel::FromVectorIcon(
          GetVectorIcon(intent), ui::kColorSysOnSurface, kIconSizeDip);
    case QuickAnswersView::Design::kMagicBoost:
      // TODO(b/335701090): update this with Magic Boost spec icon.
      return ui::ImageModel::FromVectorIcon(
          vector_icons::kGoogleColorIcon, gfx::kPlaceholderColor, kIconSizeDip);
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
                 QuickAnswersView::Design design) {
  result_view->SetFirstLineText(
      base::UTF8ToUTF16(translation_result->text_to_translate));

  if (design != QuickAnswersView::Design::kCurrent) {
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

}  // namespace

// QuickAnswersView -----------------------------------------------------------

QuickAnswersView::QuickAnswersView(
    const QuickAnswersView::Params& params,
    base::WeakPtr<QuickAnswersUiController> controller)
    : chromeos::ReadWriteCardsView(controller->GetReadWriteCardsUiController()),
      controller_(std::move(controller)),
      title_(params.title),
      design_(params.design),
      intent_(params.intent),
      is_internal_(params.is_internal),
      focus_search_(std::make_unique<chromeos::editor_menu::FocusSearch>(
          this,
          base::BindRepeating(&QuickAnswersView::GetFocusableViews,
                              base::Unretained(this)))) {
  CHECK_NE(intent_, QuickAnswersView::Intent::kUndefined);

  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorPrimaryBackground));
  SetUseDefaultFillLayout(true);

  std::unique_ptr<views::FlexLayout> main_view_layout =
      std::make_unique<views::FlexLayout>();
  main_view_layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetInteriorMargin(kMainViewInsets)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  QuickAnswersStageButton* quick_answers_stage_button;
  LoadingView* loading_view;
  RetryView* retry_view;
  ResultView* result_view;
  AddChildView(
      views::Builder<QuickAnswersStageButton>()
          .CopyAddressTo(&quick_answers_stage_button)
          .SetCallback(base::BindRepeating(
              &QuickAnswersView::SendQuickAnswersQuery, base::Unretained(this)))
          .SetAccessibleName(
              l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_VIEW_A11Y_NAME_TEXT))
          .SetLayoutManager(std::move(main_view_layout))
          .AddChild(views::Builder<views::ImageView>()
                        .SetProperty(views::kMarginsKey, kIconInsets)
                        .SetImage(GetIcon(design_, intent_)))
          .AddChild(
              views::Builder<LoadingView>()
                  .CopyAddressTo(&loading_view)
                  .SetFirstLineText(base::UTF8ToUTF16(title_))
                  .SetInteriorMargin(kContentViewInsets)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kPreferred)))
          .AddChild(
              views::Builder<RetryView>()
                  .CopyAddressTo(&retry_view)
                  .SetVisible(false)
                  .SetFirstLineText(base::UTF8ToUTF16(title_))
                  .SetInteriorMargin(kContentViewInsets)
                  .SetRetryButtonCallback(base::BindRepeating(
                      &QuickAnswersUiController::OnRetryLabelPressed,
                      controller_))
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kPreferred)))
          .AddChild(
              views::Builder<ResultView>()
                  .CopyAddressTo(&result_view)
                  .SetVisible(false)
                  .SetInteriorMargin(kContentViewInsets)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kPreferred,
                                   /*adjust_height_for_width=*/true))
                  .SetGenerateTtsCallback(base::BindRepeating(
                      &QuickAnswersView::GenerateTts, base::Unretained(this))))
          .Build());

  CHECK(quick_answers_stage_button);
  quick_answers_stage_button_ = quick_answers_stage_button;
  CHECK(loading_view);
  loading_view_ = loading_view;
  CHECK(retry_view);
  retry_view_ = retry_view;
  CHECK(result_view);
  result_view_ = result_view;

  views::ImageButton* settings_button;
  views::ImageButton* dogfood_button;
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
          .SetInsideBorderInsets(gfx::Insets(kButtonsViewMarginDip))
          .SetBetweenChildSpacing(kButtonsSpacingDip)
          .AddChild(
              views::Builder<views::ImageButton>()
                  .CopyAddressTo(&dogfood_button)
                  .SetVisible(is_internal_)
                  .SetCallback(base::BindRepeating(
                      &QuickAnswersUiController::OnReportQueryButtonPressed,
                      controller_))
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_QUICK_ANSWERS_DOGFOOD_FEEDBACK_BUTTON_TOOLTIP_TEXT))
                  .SetImageModel(
                      views::Button::STATE_NORMAL,
                      ui::ImageModel::FromVectorIcon(vector_icons::kDogfoodIcon,
                                                     ui::kColorIconSecondary,
                                                     kDogfoodButtonSizeDip)))
          .AddChild(
              views::Builder<views::ImageButton>()
                  .CopyAddressTo(&settings_button)
                  .SetCallback(base::BindRepeating(
                      &QuickAnswersUiController::OnSettingsButtonPressed,
                      controller_))
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_RICH_ANSWERS_VIEW_SETTINGS_BUTTON_A11Y_NAME_TEXT))
                  .SetImageModel(
                      views::Button::ButtonState::STATE_NORMAL,
                      ui::ImageModel::FromVectorIcon(
                          vector_icons::kSettingsOutlineIcon,
                          ui::kColorIconSecondary, kSettingsButtonSizeDip))
                  .SetProperty(views::kMarginsKey,
                               gfx::Insets(kSettingsButtonBorderDip)))
          .Build());

  CHECK(dogfood_button);
  dogfood_button_ = dogfood_button;
  CHECK(settings_button);
  settings_button_ = settings_button;

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
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

void QuickAnswersView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;

  // The view itself is not focused for retry-mode, so should not be announced
  // by the screen reader.
  if (retry_view_->GetVisible()) {
    node_data->SetNameExplicitlyEmpty();
    return;
  }

  node_data->SetName(
      l10n_util::GetStringUTF8(IDS_QUICK_ANSWERS_VIEW_A11Y_NAME_TEXT));
}

gfx::Size QuickAnswersView::GetMaximumSize() const {
  // The maximum height will be used in calculating the position of the widget
  // in `ReadWriteCardsUiController`. We need to reserve space at
  // the top since the view might expand for two-line answers.
  // Note that the width will not be used in the calculation.
  return gfx::Size(0, kMaximumHeight);
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

void QuickAnswersView::SetResult(const StructuredResult& structured_result) {
  // Check if the view (or any of its children) had focus before resetting the
  // view, so it can be restored for the updated view.
  bool pane_already_had_focus = HasFocusInside();

  SwitchTo(result_view_);

  switch (structured_result.GetResultType()) {
    case ResultType::kDefinitionResult:
      SetResultTo(result_view_, structured_result.definition_result.get());
      break;
    case ResultType::kTranslationResult:
      SetResultTo(result_view_, structured_result.translation_result.get(),
                  design_);
      break;
    case ResultType::kUnitConversionResult:
      SetResultTo(result_view_, structured_result.unit_conversion_result.get());
      break;
    case ResultType::kNoResult:
      SetNoResult(result_view_, title_);
      break;
  }

  GetViewAccessibility().SetDescription(result_view_->GetA11yDescription());

  // Restore focus if the view had one prior to updating the answer.
  if (pane_already_had_focus) {
    RequestFocus();
  } else {
    // Announce that a Quick Answer is available.
    GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_VIEW_A11Y_INFO_ALERT_TEXT));
  }
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
END_METADATA

}  // namespace quick_answers
