// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"

#include <string_view>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/loading_view.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_stage_button.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_text_label.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"
#include "chrome/browser/ui/views/editor_menu/utils/focus_search.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
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
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/painter.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

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
constexpr auto kRichCardRedesignContentViewInsets =
    gfx::Insets::TLBR(8, 0, 8, 12);
constexpr int kMaxRows = 3;

// Google icon.
constexpr int kGoogleIconSizeDip = 16;
constexpr auto kGoogleIconInsets = gfx::Insets::TLBR(10, 10, 0, 10);

// Result type icon.
constexpr int kResultTypeIconContainerRadius = 18;
constexpr int kResultTypeIconSizeDip = 12;
constexpr auto kResultTypeIconContainerInsets = gfx::Insets::TLBR(8, 12, 4, 8);
constexpr auto kResultTypeIconCircleInsets = gfx::Insets::TLBR(4, 4, 4, 4);

// Info icon.
constexpr int kDogfoodIconSizeDip = 20;
constexpr int kDogfoodIconBorderDip = 8;

// Spacing between lines in the main view.
constexpr int kLineSpacingDip = 4;
constexpr int kDefaultLineHeightDip = 20;

// Buttons view.
constexpr int kButtonsViewMarginDip = 4;
constexpr int kButtonsSpacingDip = 4;
constexpr int kDogfoodButtonSizeDip = 20;
constexpr int kSettingsButtonSizeDip = 14;
constexpr int kSettingsButtonBorderDip = 3;

// Phonetics audio button.
constexpr auto kPhoneticsAudioButtonMarginInsets =
    gfx::Insets::TLBR(0, 4, 0, 4);
constexpr int kPhoneticsAudioButtonSizeDip = 14;
constexpr int kPhoneticsAudioButtonBorderDip = 3;

// ReportQueryView.
constexpr int kReportQueryButtonMarginDip = 16;
constexpr int kReportQueryViewFontSize = 12;

// Expansion affordance indicator.
constexpr int kExpansionIndicatorLabelFontSize = 12;
constexpr int kExpansionIndicatorIconSizeDip = 12;
constexpr int kExpansionIndicatorIconBorderDip = 4;
constexpr int kExpansionIndicatorSizeDip = 72;
constexpr auto kExpansionIndicatorViewInsets = gfx::Insets::TLBR(4, 8, 16, 12);

gfx::Insets GetContentViewInsets() {
  if (chromeos::features::IsQuickAnswersRichCardEnabled()) {
    return kRichCardRedesignContentViewInsets;
  }
  return kContentViewInsets;
}

class ReportQueryView : public views::Button {
  METADATA_HEADER(ReportQueryView, views::Button)

 public:
  static constexpr size_t kMaximumHeight = kDogfoodIconBorderDip * 2;

  explicit ReportQueryView(PressedCallback callback)
      : Button(std::move(callback)) {
    SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_QUICK_ANSWERS_VIEW_REPORT_QUERY_REPORT_LABEL));

    SetBackground(views::CreateThemedSolidBackground(
        kColorQuickAnswersReportQueryButtonBackground));

    auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetMainAxisAlignment(views::LayoutAlignment::kStart);

    dogfood_icon_ = AddChildView(std::make_unique<views::ImageView>());
    dogfood_icon_->SetBorder(views::CreateEmptyBorder(kDogfoodIconBorderDip));

    description_label_ = AddChildView(std::make_unique<Label>(
        l10n_util::GetStringUTF16(
            IDS_QUICK_ANSWERS_VIEW_REPORT_QUERY_INTERNAL_LABEL),
        Label::CustomFont{gfx::FontList(
            {quick_answers::kGoogleSansFont}, gfx::Font::ITALIC,
            kReportQueryViewFontSize, gfx::Font::Weight::NORMAL)}));
    description_label_->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);

    report_label_ = AddChildView(std::make_unique<Label>(
        l10n_util::GetStringUTF16(
            IDS_QUICK_ANSWERS_VIEW_REPORT_QUERY_REPORT_LABEL),
        Label::CustomFont{gfx::FontList(
            {quick_answers::kGoogleSansFont}, gfx::Font::NORMAL,
            kReportQueryViewFontSize, gfx::Font::Weight::MEDIUM)}));
    report_label_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded)
            .WithAlignment(views::LayoutAlignment::kEnd));
    report_label_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, 0, 0, kReportQueryButtonMarginDip));
  }

  // Disallow copy and assign.
  ReportQueryView(const ReportQueryView&) = delete;
  ReportQueryView& operator=(const ReportQueryView&) = delete;

  ~ReportQueryView() override = default;

  // views::View:
  void OnThemeChanged() override {
    views::Button::OnThemeChanged();

    dogfood_icon_->SetImage(ui::ImageModel::FromVectorIcon(
        vector_icons::kDogfoodIcon,
        kColorQuickAnswersReportQueryButtonForeground, kDogfoodIconSizeDip));
    description_label_->SetEnabledColorId(
        kColorQuickAnswersReportQueryButtonForeground);
    report_label_->SetEnabledColorId(
        kColorQuickAnswersReportQueryButtonForeground);
  }

 private:
  raw_ptr<views::ImageView> dogfood_icon_ = nullptr;
  raw_ptr<views::Label> description_label_ = nullptr;
  raw_ptr<views::Label> report_label_ = nullptr;
};

BEGIN_METADATA(ReportQueryView)
END_METADATA

// Maximum height QuickAnswersView can expand to.
int MaximumViewHeight(bool is_internal) {
  return kMainViewInsets.height() + GetContentViewInsets().height() +
         kMaxRows * kDefaultLineHeightDip + (kMaxRows - 1) * kLineSpacingDip +
         (is_internal ? ReportQueryView::kMaximumHeight : 0);
}

// `MaybeASingleQuickAnswersTextLabel` returns a pointer of
// `QuickAnswersTextLabel` if `container` only contains a
// `QuickAnswersTextLabel`. Otherwise, this returns `nullptr`.
quick_answers::QuickAnswersTextLabel* MaybeASingleQuickAnswersTextLabel(
    views::View* container) {
  if (container->children().size() != 1) {
    return nullptr;
  }

  // `AsViewClass` returns `nullptr` if a view is not a specified class.
  return views::AsViewClass<quick_answers::QuickAnswersTextLabel>(
      container->children().front());
}

views::Builder<views::FlexLayoutView> DefaultResultTypeIconBuilder(
    views::ImageView** result_type_icon) {
  return views::Builder<views::FlexLayoutView>()
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetInteriorMargin(kResultTypeIconContainerInsets)
      .AddChild(
          views::Builder<views::FlexLayoutView>()
              .SetBackground(views::CreateThemedRoundedRectBackground(
                  ui::kColorSysPrimary, kResultTypeIconContainerRadius))
              .SetBorder(views::CreateEmptyBorder(kResultTypeIconCircleInsets))
              .AddChild(views::Builder<views::ImageView>()
                            .SetImage(ui::ImageModel::FromVectorIcon(
                                GetResultTypeIcon(ResultType::kNoResult),
                                ui::kColorSysBaseContainerElevated,
                                /*icon_size=*/kResultTypeIconSizeDip))
                            .CopyAddressTo(result_type_icon)));
}

views::Builder<views::FlexLayoutView> GoogleIconBuilder() {
  return views::Builder<views::FlexLayoutView>()
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .AddChild(views::Builder<views::ImageView>()
                    .SetBorder(views::CreateEmptyBorder(kGoogleIconInsets))
                    .SetImage(ui::ImageModel::FromVectorIcon(
                        vector_icons::kGoogleColorIcon, gfx::kPlaceholderColor,
                        kGoogleIconSizeDip)));
}

}  // namespace

namespace quick_answers {

// QuickAnswersView -----------------------------------------------------------

QuickAnswersView::QuickAnswersView(
    const std::string& title,
    bool is_internal,
    base::WeakPtr<QuickAnswersUiController> controller)
    : chromeos::ReadWriteCardsView(controller->GetReadWriteCardsUiController()),
      controller_(std::move(controller)),
      title_(title),
      is_internal_(is_internal),
      focus_search_(std::make_unique<chromeos::editor_menu::FocusSearch>(
          this,
          base::BindRepeating(&QuickAnswersView::GetFocusableViews,
                              base::Unretained(this)))) {
  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorPrimaryBackground));
  SetUseDefaultFillLayout(true);

  bool is_rich_answers_enabled =
      chromeos::features::IsQuickAnswersRichCardEnabled();

  std::unique_ptr<views::FlexLayout> main_view_layout =
      std::make_unique<views::FlexLayout>();
  main_view_layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetInteriorMargin(kMainViewInsets);

  QuickAnswersStageButton* main_view;
  views::View* content_view;
  views::ImageView* result_type_icon;

  base_view_.SetView(AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
          .AddChild(
              views::Builder<QuickAnswersStageButton>()
                  .SetCallback(base::BindRepeating(
                      &QuickAnswersView::SendQuickAnswersQuery,
                      base::Unretained(this)))
                  .SetAccessibleName(l10n_util::GetStringUTF16(
                      IDS_QUICK_ANSWERS_VIEW_A11Y_NAME_TEXT))
                  .SetLayoutManager(std::move(main_view_layout))
                  .CopyAddressTo(&main_view)
                  .AddChild(
                      is_rich_answers_enabled
                          ? DefaultResultTypeIconBuilder(&result_type_icon)
                          : GoogleIconBuilder())
                  .AddChild(
                      views::Builder<LoadingView>()
                          .SetFirstLineText(base::UTF8ToUTF16(title_))
                          .SetInteriorMargin(GetContentViewInsets())
                          .SetProperty(
                              views::kFlexBehaviorKey,
                              views::FlexSpecification(
                                  views::MinimumFlexSizeRule::kScaleToZero,
                                  views::MaximumFlexSizeRule::kPreferred))
                          .CopyAddressTo(&content_view)))
          .Build()));

  CHECK(main_view);
  main_view_.SetView(main_view);
  CHECK(content_view);
  content_view_.SetView(content_view);

  if (is_rich_answers_enabled) {
    CHECK(result_type_icon);
    result_type_icon_ = result_type_icon;
  }

  if (!is_rich_answers_enabled) {
    // Add util buttons in the top-right corner.
    AddFrameButtons();
  }

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
}

QuickAnswersView::~QuickAnswersView() = default;

void QuickAnswersView::RequestFocus() {
  // When the Quick Answers view is focused, we actually want `main_view_`
  // to have the focus for highlight and selection purposes.
  main_view_.view()->RequestFocus();
}

bool QuickAnswersView::HasFocus() const {
  // When the Quick Answers view is focused, `main_view_` should have
  // the focus.
  return main_view_.view()->HasFocus();
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

void QuickAnswersView::OnThemeChanged() {
  views::View::OnThemeChanged();

  if (settings_button_) {
    settings_button_->SetImageModel(
        views::Button::ButtonState::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            vector_icons::kSettingsOutlineIcon,
            GetColorProvider()->GetColor(ui::kColorIconSecondary),
            kSettingsButtonSizeDip));
  }

  if (dogfood_feedback_button_) {
    dogfood_feedback_button_->SetImageModel(
        views::Button::ButtonState::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            vector_icons::kDogfoodIcon,
            GetColorProvider()->GetColor(ui::kColorIconSecondary),
            kDogfoodButtonSizeDip));
  }
}

views::FocusTraversable* QuickAnswersView::GetPaneFocusTraversable() {
  return focus_search_.get();
}

void QuickAnswersView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;

  // The view itself is not focused for retry-mode, so should not be announced
  // by the screen reader.
  if (retry_label_) {
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
  return gfx::Size(0, MaximumViewHeight(is_internal_));
}

void QuickAnswersView::UpdateBoundsForQuickAnswers() {
  // Multi-line labels need to be resized to be compatible with bounds width.
  // TODO(b/331271987): Remove this. This should be handled by a layout manager.
  if (first_answer_label_) {
    first_answer_label_->SetMaximumWidth(GetLabelWidth(/*is_title=*/false));
  }
}

void QuickAnswersView::SendQuickAnswersQuery() {
  if (controller_) {
    controller_->OnQuickAnswersViewPressed();
  }
}

void QuickAnswersView::UpdateView(const QuickAnswer& quick_answer) {
  retry_label_ = nullptr;

  UpdateQuickAnswerResult(quick_answer);
}

void QuickAnswersView::ShowRetryView() {
  if (retry_label_) {
    return;
  }

  ResetContentView();

  // TODO(b/335701090): Utilize main_view_ for handling OnRetryLabelPressed.
  main_view_.view()->SetBackground(nullptr);

  // Add title.
  auto* title_label = content_view_.view()->AddChildView(
      std::make_unique<quick_answers::QuickAnswersTextLabel>(
          QuickAnswerText(title_)));
  title_label->SetMaximumWidthSingleLine(GetLabelWidth(/*is_title=*/true));

  // Add error label.
  std::vector<std::unique_ptr<QuickAnswerUiElement>> description_labels;
  description_labels.push_back(std::make_unique<QuickAnswerResultText>(
      l10n_util::GetStringUTF8(IDS_QUICK_ANSWERS_VIEW_NETWORK_ERROR)));
  auto* description_container =
      AddHorizontalUiElements(content_view_.view(), description_labels);

  // Add retry label.
  retry_label_ =
      description_container->AddChildView(std::make_unique<views::LabelButton>(
          base::BindRepeating(&QuickAnswersUiController::OnRetryLabelPressed,
                              controller_),
          l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_VIEW_RETRY)));
  retry_label_->SetEnabledTextColors(
      GetColorProvider()->GetColor(ui::kColorProgressBar));
  retry_label_->SetRequestFocusOnPress(true);
  retry_label_->button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  retry_label_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_QUICK_ANSWERS_VIEW_A11Y_RETRY_LABEL_NAME_TEMPLATE,
      l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_VIEW_A11Y_NAME_TEXT)));
  retry_label_->GetViewAccessibility().SetDescription(
      l10n_util::GetStringUTF8(IDS_QUICK_ANSWERS_VIEW_A11Y_RETRY_LABEL_DESC));
}

ui::ImageModel QuickAnswersView::GetIconImageModelForTesting() {
  return result_type_icon_ ? result_type_icon_->GetImageModel()
                           : ui::ImageModel();
}

void QuickAnswersView::AddFrameButtons() {
  auto* buttons_view = AddChildView(std::make_unique<views::View>());
  auto* layout =
      buttons_view->SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetInteriorMargin(gfx::Insets(kButtonsViewMarginDip))
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, kButtonsSpacingDip, 0, 0));

  if (is_internal_) {
    dogfood_feedback_button_ = buttons_view->AddChildView(
        std::make_unique<views::ImageButton>(base::BindRepeating(
            &QuickAnswersUiController::OnReportQueryButtonPressed,
            controller_)));
    dogfood_feedback_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_QUICK_ANSWERS_DOGFOOD_FEEDBACK_BUTTON_TOOLTIP_TEXT));
  }

  settings_button_ = buttons_view->AddChildView(
      std::make_unique<views::ImageButton>(base::BindRepeating(
          &QuickAnswersUiController::OnSettingsButtonPressed, controller_)));
  settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_RICH_ANSWERS_VIEW_SETTINGS_BUTTON_A11Y_NAME_TEXT));
  settings_button_->SetBorder(
      views::CreateEmptyBorder(kSettingsButtonBorderDip));
}

bool QuickAnswersView::ShouldAddPhoneticsAudioButton(ResultType result_type,
                                                     GURL phonetics_audio,
                                                     bool tts_audio_enabled) {
  if (chromeos::features::IsQuickAnswersRichCardEnabled()) {
    return false;
  }

  if (result_type != ResultType::kDefinitionResult) {
    return false;
  }

  return !phonetics_audio.is_empty() || tts_audio_enabled;
}

void QuickAnswersView::AddPhoneticsAudioButton(
    const PhoneticsInfo& phonetics_info,
    View* container) {
  auto* phonetics_audio_view =
      container->AddChildView(std::make_unique<views::View>());

  auto* layout = phonetics_audio_view->SetLayoutManager(
      std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(kPhoneticsAudioButtonMarginInsets)
      .SetCrossAxisAlignment(views::LayoutAlignment::kEnd);
  phonetics_audio_button_ =
      phonetics_audio_view->AddChildView(std::make_unique<views::ImageButton>(
          base::BindRepeating(&QuickAnswersView::OnPhoneticsAudioButtonPressed,
                              base::Unretained(this), phonetics_info)));
  phonetics_audio_button_->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(
          vector_icons::kVolumeUpIcon,
          GetColorProvider()->GetColor(ui::kColorButtonBackgroundProminent),
          kPhoneticsAudioButtonSizeDip));
  phonetics_audio_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_RICH_ANSWERS_VIEW_PHONETICS_BUTTON_A11Y_NAME_TEXT));
  phonetics_audio_button_->SetBorder(
      views::CreateEmptyBorder(kPhoneticsAudioButtonBorderDip));
}

int QuickAnswersView::GetLabelWidth(bool is_title) {
  int label_width = context_menu_bounds().width() - kMainViewInsets.width() -
                    GetContentViewInsets().width() - kGoogleIconInsets.width() -
                    kGoogleIconSizeDip;

  // If the rich card feature flag is enabled, leave additional space
  // for the expansion affordance indicator.
  // This only applies to non-title text labels.
  if (chromeos::features::IsQuickAnswersRichCardEnabled() && !is_title) {
    return label_width - kExpansionIndicatorSizeDip;
  }

  return label_width;
}

void QuickAnswersView::ResetContentView() {
  content_view_.view()->RemoveAllChildViews();
  first_answer_label_ = nullptr;
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

void QuickAnswersView::UpdateQuickAnswerResult(
    const QuickAnswer& quick_answer) {
  // Check if the view (or any of its children) had focus before resetting the
  // view, so it can be restored for the updated view.
  bool pane_already_had_focus = HasFocusInside();
  ResetContentView();

  if (report_query_view_) {
    RemoveChildViewT(report_query_view_.view());
  }

  // Update the icon representing the quick answers result type if it's shown.
  // In the case that the rich card feature is not enabled, this icon is null
  // and the google icon is being shown instead.
  if (result_type_icon_ && quick_answer.result_type != ResultType::kNoResult) {
    result_type_icon_->SetImage(ui::ImageModel::FromVectorIcon(
        GetResultTypeIcon(quick_answer.result_type),
        ui::kColorSysBaseContainerElevated,
        /*icon_size=*/kResultTypeIconSizeDip));
  }

  // Add title.
  View* title_view =
      AddHorizontalUiElements(content_view_.view(), quick_answer.title);
  auto* title_label = static_cast<Label*>(title_view->children().front());
  title_label->SetMaximumWidthSingleLine(GetLabelWidth(/*is_title=*/true));

  // Add phonetics audio button for definition results.
  if (ShouldAddPhoneticsAudioButton(
          quick_answer.result_type, quick_answer.phonetics_info.phonetics_audio,
          quick_answer.phonetics_info.tts_audio_enabled)) {
    AddPhoneticsAudioButton(quick_answer.phonetics_info, title_view);
  }

  // Add first row answer.
  if (!quick_answer.first_answer_row.empty()) {
    views::View* first_answer_row = AddHorizontalUiElements(
        content_view_.view(), quick_answer.first_answer_row);

    QuickAnswersTextLabel* quick_answers_text_label =
        MaybeASingleQuickAnswersTextLabel(first_answer_row);
    if (quick_answers_text_label) {
      GetViewAccessibility().SetDescription(l10n_util::GetStringFUTF16(
          IDS_QUICK_ANSWERS_VIEW_A11Y_INFO_DESC_TEMPLATE_V2,
          title_label->GetText(), quick_answers_text_label->GetText()));

      quick_answers_text_label->SetMultiLine(true);
      // Max lines is kMaxRows-1. 1 is for the title row.
      quick_answers_text_label->SetMaxLines(kMaxRows - 1);

      // TODO(b/331271987): Remove this caching and manual re-layout.
      first_answer_label_ = quick_answers_text_label;
      UpdateBoundsForQuickAnswers();
    }
  }

  // Restore focus if the view had one prior to updating the answer.
  if (pane_already_had_focus) {
    RequestFocus();
  } else {
    // Announce that a Quick Answer is available.
    GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_VIEW_A11Y_INFO_ALERT_TEXT));
  }

  if (quick_answer.result_type == ResultType::kNoResult && is_internal_) {
    CHECK(base_view_.view());
    report_query_view_.SetView(base_view_.view()->AddChildView(
        std::make_unique<ReportQueryView>(base::BindRepeating(
            &QuickAnswersUiController::OnReportQueryButtonPressed,
            controller_))));
  }

  if (chromeos::features::IsQuickAnswersRichCardEnabled() &&
      quick_answer.result_type != ResultType::kNoResult) {
    // Show the expansion affordance indicator if rich card view is available.
    auto* expansion_indicator_view =
        AddChildView(views::Builder<views::FlexLayoutView>()
                         .SetOrientation(views::LayoutOrientation::kHorizontal)
                         .SetInteriorMargin(kExpansionIndicatorViewInsets)
                         .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
                         .SetCrossAxisAlignment(views::LayoutAlignment::kEnd)
                         .Build());

    auto* expansion_indicator_label =
        expansion_indicator_view->AddChildView(std::make_unique<Label>(
            l10n_util::GetStringUTF16(
                IDS_QUICK_ANSWERS_VIEW_EXPANSION_INDICATOR_LABEL),
            Label::CustomFont{gfx::FontList(
                {quick_answers::kRobotoFont}, gfx::Font::NORMAL,
                kExpansionIndicatorLabelFontSize, gfx::Font::Weight::MEDIUM)}));
    expansion_indicator_label->SetEnabledColorId(ui::kColorSysPrimary);

    auto* expansion_indicator_icon = expansion_indicator_view->AddChildView(
        std::make_unique<views::ImageView>());
    expansion_indicator_icon->SetImage(ui::ImageModel::FromVectorIcon(
        vector_icons::kCaretDownIcon,
        GetColorProvider()->GetColor(ui::kColorSysPrimary),
        /*icon_size=*/kExpansionIndicatorIconSizeDip));
    expansion_indicator_icon->SetBorder(
        views::CreateEmptyBorder(kExpansionIndicatorIconBorderDip));
  }
}

std::vector<views::View*> QuickAnswersView::GetFocusableViews() {
  std::vector<views::View*> focusable_views;
  // The main view does not gain focus for retry-view and transfers it to the
  // retry-label, and so is not included when this is the case.
  if (!retry_label_) {
    focusable_views.push_back(main_view_.view());
  }
  if (dogfood_feedback_button_ && dogfood_feedback_button_->GetVisible()) {
    focusable_views.push_back(dogfood_feedback_button_);
  }
  if (settings_button_ && settings_button_->GetVisible()) {
    focusable_views.push_back(settings_button_);
  }
  if (phonetics_audio_button_ && phonetics_audio_button_->GetVisible()) {
    focusable_views.push_back(phonetics_audio_button_);
  }
  if (retry_label_ && retry_label_->GetVisible()) {
    focusable_views.push_back(retry_label_);
  }
  if (report_query_view_ && report_query_view_.view()->GetVisible()) {
    focusable_views.push_back(report_query_view_.view());
  }
  return focusable_views;
}

void QuickAnswersView::OnPhoneticsAudioButtonPressed(
    const PhoneticsInfo& phonetics_info) {
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

BEGIN_METADATA(QuickAnswersView)
END_METADATA

}  // namespace quick_answers
