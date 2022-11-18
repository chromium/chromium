// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_pre_target_handler.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/browser/speech/tts_controller_impl.h"
#include "content/public/browser/tts_utterance.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/display/screen.h"
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
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/painter.h"
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

// Spacing between this view and the anchor view.
constexpr int kMarginDip = 10;

constexpr auto kMainViewInsets = gfx::Insets::VH(4, 0);
constexpr auto kContentViewInsets = gfx::Insets::TLBR(8, 0, 8, 16);
constexpr int kMaxRows = 3;

// Google icon.
constexpr int kGoogleIconSizeDip = 16;
constexpr auto kGoogleIconInsets = gfx::Insets::TLBR(10, 10, 0, 10);

// Info icon.
constexpr int kDogfoodIconSizeDip = 20;
constexpr int kDogfoodIconBorderDip = 8;

// Spacing between lines in the main view.
constexpr int kLineSpacingDip = 4;
constexpr int kDefaultLineHeightDip = 20;

// Spacing between labels in the horizontal elements view.
constexpr int kLabelSpacingDip = 2;

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
constexpr char kGoogleSansFont[] = "Google Sans";
constexpr int kReportQueryButtonMarginDip = 16;
constexpr int kReportQueryViewFontSize = 12;

// TTS audio.
constexpr char kGoogleTtsEngineId[] = "com.google.android.tts";

// Maximum height QuickAnswersView can expand to.
int MaximumViewHeight() {
  return kMainViewInsets.height() + kContentViewInsets.height() +
         kMaxRows * kDefaultLineHeightDip + (kMaxRows - 1) * kLineSpacingDip;
}

class QuickAnswersTextLabel : public views::Label {
 public:
  METADATA_HEADER(QuickAnswersTextLabel);

  explicit QuickAnswersTextLabel(QuickAnswerText quick_answers_text)
      : Label(quick_answers_text.text),
        quick_answers_text_(quick_answers_text) {
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  }

  QuickAnswersTextLabel(const QuickAnswersTextLabel&) = delete;
  QuickAnswersTextLabel& operator=(const QuickAnswersTextLabel&) = delete;

  ~QuickAnswersTextLabel() override = default;

  // views::View:
  void OnThemeChanged() override {
    views::Label::OnThemeChanged();
    SetEnabledColor(GetColorProvider()->GetColor(quick_answers_text_.color_id));
  }

 private:
  QuickAnswerText quick_answers_text_;
};

BEGIN_METADATA(QuickAnswersTextLabel, views::Label)
END_METADATA

// Adds the list of |QuickAnswerUiElement| horizontally to the container.
View* AddHorizontalUiElements(
    const std::vector<std::unique_ptr<QuickAnswerUiElement>>& elements,
    View* container) {
  auto* labels_container =
      container->AddChildView(std::make_unique<views::View>());
  auto* layout =
      labels_container->SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, 0, 0, kLabelSpacingDip));

  for (const auto& element : elements) {
    switch (element->type) {
      case QuickAnswerUiElementType::kText:
        labels_container->AddChildView(std::make_unique<QuickAnswersTextLabel>(
            *static_cast<QuickAnswerText*>(element.get())));
        break;
      case QuickAnswerUiElementType::kImage:
        // TODO(yanxiao): Add image view
        break;
      default:
        break;
    }
  }

  return labels_container;
}

class MainView : public views::Button {
 public:
  METADATA_HEADER(MainView);

  explicit MainView(PressedCallback callback) : Button(std::move(callback)) {
    SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_VIEW_A11Y_NAME_TEXT));
    SetInstallFocusRingOnFocus(false);
    set_suppress_default_focus_handling();

    // This is because waiting for mouse-release to fire buttons would be too
    // late, since mouse-press dismisses the menu.
    button_controller()->set_notify_action(
        views::ButtonController::NotifyAction::kOnPress);
  }

  // Disallow copy and assign.
  MainView(const MainView&) = delete;
  MainView& operator=(const MainView&) = delete;

  ~MainView() override = default;

 private:
  // views::View:
  void OnFocus() override { SetBackgroundState(true); }
  void OnBlur() override { SetBackgroundState(false); }
  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    SetBackground(views::CreateSolidBackground(
        GetColorProvider()->GetColor(ui::kColorPrimaryBackground)));
  }

  // views::Button:
  void StateChanged(views::Button::ButtonState old_state) override {
    Button::StateChanged(old_state);
    const bool hovered = GetState() == Button::STATE_HOVERED;
    if (hovered || (GetState() == Button::STATE_NORMAL))
      SetBackgroundState(hovered);
  }

  void SetBackgroundState(bool highlight) {
    if (highlight) {
      SetBackground(views::CreateBackgroundFromPainter(
          views::Painter::CreateSolidRoundRectPainter(
              GetColorProvider()->GetColor(
                  ui::kColorMenuItemBackgroundHighlighted),
              /*radius=*/0, kMainViewInsets)));
    } else {
      SetBackground(views::CreateSolidBackground(
          GetColorProvider()->GetColor(ui::kColorPrimaryBackground)));
    }
  }
};

BEGIN_METADATA(MainView, views::Button)
END_METADATA

class ReportQueryView : public views::Button {
 public:
  METADATA_HEADER(ReportQueryView);

  explicit ReportQueryView(PressedCallback callback)
      : Button(std::move(callback)) {
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
        Label::CustomFont{gfx::FontList({kGoogleSansFont}, gfx::Font::ITALIC,
                                        kReportQueryViewFontSize,
                                        gfx::Font::Weight::NORMAL)}));
    description_label_->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);

    report_label_ = AddChildView(std::make_unique<Label>(
        l10n_util::GetStringUTF16(
            IDS_QUICK_ANSWERS_VIEW_REPORT_QUERY_REPORT_LABEL),
        Label::CustomFont{gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL,
                                        kReportQueryViewFontSize,
                                        gfx::Font::Weight::MEDIUM)}));
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

    const auto* const color_provider = GetColorProvider();
    const SkColor foreground_color =
        color_provider->GetColor(kColorQuickAnswersReportQueryButtonForeground);
    dogfood_icon_->SetImage(gfx::CreateVectorIcon(
        vector_icons::kDogfoodIcon, kDogfoodIconSizeDip, foreground_color));
    description_label_->SetEnabledColor(foreground_color);
    report_label_->SetEnabledColor(foreground_color);
  }

 private:
  raw_ptr<views::ImageView> dogfood_icon_ = nullptr;
  raw_ptr<views::Label> description_label_ = nullptr;
  raw_ptr<views::Label> report_label_ = nullptr;
};

BEGIN_METADATA(ReportQueryView, views::Button)
END_METADATA

// The lifetime of instances of this class is manually bound to the lifetime of
// the associated TtsUtterance. See OnTtsEvent.
class QuickAnswersUtteranceEventDelegate
    : public content::UtteranceEventDelegate {
 public:
  QuickAnswersUtteranceEventDelegate() = default;
  ~QuickAnswersUtteranceEventDelegate() override = default;

  // UtteranceEventDelegate methods:
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int char_length,
                  const std::string& error_message) override {
    // For quick answers, the TTS events of interest are START, END, and ERROR.
    switch (event_type) {
      case content::TTS_EVENT_START:
        quick_answers::RecordTtsEngineEvent(
            quick_answers::TtsEngineEvent::TTS_EVENT_START);
        break;
      case content::TTS_EVENT_END:
        quick_answers::RecordTtsEngineEvent(
            quick_answers::TtsEngineEvent::TTS_EVENT_END);
        break;
      case content::TTS_EVENT_ERROR:
        VLOG(1) << __func__ << ": " << error_message;
        quick_answers::RecordTtsEngineEvent(
            quick_answers::TtsEngineEvent::TTS_EVENT_ERROR);
        break;
      case content::TTS_EVENT_WORD:
      case content::TTS_EVENT_SENTENCE:
      case content::TTS_EVENT_MARKER:
      case content::TTS_EVENT_INTERRUPTED:
      case content::TTS_EVENT_CANCELLED:
      case content::TTS_EVENT_PAUSE:
      case content::TTS_EVENT_RESUME:
        // Group the remaining TTS events that aren't of interest together
        // into an unspecified "other" category.
        quick_answers::RecordTtsEngineEvent(
            quick_answers::TtsEngineEvent::TTS_EVENT_OTHER);
        break;
    }

    if (utterance->IsFinished())
      delete this;
  }
};

}  // namespace

// QuickAnswersView -----------------------------------------------------------

QuickAnswersView::QuickAnswersView(
    const gfx::Rect& anchor_view_bounds,
    const std::string& title,
    bool is_internal,
    base::WeakPtr<QuickAnswersUiController> controller)
    : anchor_view_bounds_(anchor_view_bounds),
      controller_(std::move(controller)),
      title_(title),
      is_internal_(is_internal),
      quick_answers_view_handler_(
          std::make_unique<QuickAnswersPreTargetHandler>(this)),
      focus_search_(std::make_unique<QuickAnswersFocusSearch>(
          this,
          base::BindRepeating(&QuickAnswersView::GetFocusableViews,
                              base::Unretained(this)))) {
  InitLayout();
  InitWidget();

  // Focus.
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();

  // Allow tooltips to be shown despite menu-controller owning capture.
  GetWidget()->SetNativeWindowProperty(
      views::TooltipManager::kGroupingPropertyKey,
      reinterpret_cast<void*>(views::MenuConfig::kMenuControllerGroupingId));
}

QuickAnswersView::~QuickAnswersView() = default;

const char* QuickAnswersView::GetClassName() const {
  return "QuickAnswersView";
}

void QuickAnswersView::OnFocus() {
  View* wants_focus = focus_search_->FindNextFocusableView(
      nullptr, views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kCheckStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kSkipAnchoredDialog, nullptr,
      nullptr);
  if (wants_focus != this)
    wants_focus->RequestFocus();
  else
    NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
}

void QuickAnswersView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(
      GetColorProvider()->GetColor(ui::kColorPrimaryBackground)));
  if (settings_button_) {
    settings_button_->SetImage(
        views::Button::ButtonState::STATE_NORMAL,
        gfx::CreateVectorIcon(
            vector_icons::kSettingsOutlineIcon, kSettingsButtonSizeDip,
            GetColorProvider()->GetColor(ui::kColorIconSecondary)));
  }

  if (dogfood_feedback_button_) {
    dogfood_feedback_button_->SetImage(
        views::Button::ButtonState::STATE_NORMAL,
        gfx::CreateVectorIcon(
            vector_icons::kDogfoodIcon, kDogfoodButtonSizeDip,
            GetColorProvider()->GetColor(ui::kColorIconSecondary)));
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

void QuickAnswersView::SendQuickAnswersQuery() {
  if (controller_)
    controller_->OnQuickAnswersViewPressed();
}

void QuickAnswersView::UpdateAnchorViewBounds(
    const gfx::Rect& anchor_view_bounds) {
  anchor_view_bounds_ = anchor_view_bounds;
  UpdateBounds();
}

void QuickAnswersView::UpdateView(const gfx::Rect& anchor_view_bounds,
                                  const QuickAnswer& quick_answer) {
  has_second_row_answer_ = !quick_answer.second_answer_row.empty();
  anchor_view_bounds_ = anchor_view_bounds;
  retry_label_ = nullptr;

  UpdateQuickAnswerResult(quick_answer);
  UpdateBounds();
}

void QuickAnswersView::ShowRetryView() {
  if (retry_label_)
    return;

  ResetContentView();
  main_view_->SetBackground(nullptr);

  // Add title.
  auto* title_label = content_view_->AddChildView(
      std::make_unique<QuickAnswersTextLabel>(QuickAnswerText(title_)));
  title_label->SetMaximumWidthSingleLine(GetLabelWidth());

  // Add error label.
  std::vector<std::unique_ptr<QuickAnswerUiElement>> description_labels;
  description_labels.push_back(std::make_unique<QuickAnswerResultText>(
      l10n_util::GetStringUTF8(IDS_QUICK_ANSWERS_VIEW_NETWORK_ERROR)));
  auto* description_container =
      AddHorizontalUiElements(description_labels, content_view_);

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
  retry_label_->GetViewAccessibility().OverrideDescription(
      l10n_util::GetStringUTF8(IDS_QUICK_ANSWERS_VIEW_A11Y_RETRY_LABEL_DESC));
}

void QuickAnswersView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  base_view_ = AddChildView(std::make_unique<View>());
  auto* base_layout =
      base_view_->SetLayoutManager(std::make_unique<views::FlexLayout>());
  base_layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  main_view_ =
      base_view_->AddChildView(std::make_unique<MainView>(base::BindRepeating(
          &QuickAnswersView::SendQuickAnswersQuery, base::Unretained(this))));
  auto* layout =
      main_view_->SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetInteriorMargin(kMainViewInsets)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  // Add branding icon.
  AddGoogleIcon();

  AddContentView();

  // Add util buttons in the top-right corner.
  AddFrameButtons();
}

void QuickAnswersView::InitWidget() {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;

  // Parent the widget to the owner of the menu.
  auto* active_menu_controller = views::MenuController::GetActiveInstance();
  DCHECK(active_menu_controller && active_menu_controller->owner());
  params.parent = active_menu_controller->owner()->GetNativeView();
  params.child = true;

  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->SetContentsView(this);
  UpdateBounds();
}

void QuickAnswersView::AddContentView() {
  // Add content view.
  content_view_ = main_view_->AddChildView(std::make_unique<View>());
  auto* layout =
      content_view_->SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(kContentViewInsets)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, 0, kLineSpacingDip, 0));
  auto* title_label = content_view_->AddChildView(
      std::make_unique<QuickAnswersTextLabel>(QuickAnswerText(title_)));
  title_label->SetMaximumWidthSingleLine(GetLabelWidth());
  std::string loading =
      l10n_util::GetStringUTF8(IDS_QUICK_ANSWERS_VIEW_LOADING);
  content_view_->AddChildView(
      std::make_unique<QuickAnswersTextLabel>(QuickAnswerResultText(loading)));
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
      IDS_QUICK_ANSWERS_SETTINGS_BUTTON_TOOLTIP_TEXT));
  settings_button_->SetBorder(
      views::CreateEmptyBorder(kSettingsButtonBorderDip));
}

void QuickAnswersView::AddPhoneticsAudioButton(
    const PhoneticsInfo& phonetics_info,
    View* container) {
  auto* phonetics_audio_view =
      container->AddChildView(std::make_unique<views::View>());

  // Setup an invisible web view to play phonetics audio.
  phonetics_audio_web_view_ = container->AddChildView(
      std::make_unique<views::WebView>(ProfileManager::GetActiveUserProfile()));
  phonetics_audio_web_view_->SetVisible(false);

  auto* layout = phonetics_audio_view->SetLayoutManager(
      std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(kPhoneticsAudioButtonMarginInsets)
      .SetCrossAxisAlignment(views::LayoutAlignment::kEnd);
  phonetics_audio_button_ =
      phonetics_audio_view->AddChildView(std::make_unique<views::ImageButton>(
          base::BindRepeating(&QuickAnswersView::OnPhoneticsAudioButtonPressed,
                              base::Unretained(this), phonetics_info)));
  phonetics_audio_button_->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(
          vector_icons::kVolumeUpIcon, kPhoneticsAudioButtonSizeDip,
          GetColorProvider()->GetColor(ui::kColorButtonBackgroundProminent)));
  phonetics_audio_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_QUICK_ANSWERS_PHONETICS_BUTTON_TOOLTIP_TEXT));
  phonetics_audio_button_->SetBorder(
      views::CreateEmptyBorder(kPhoneticsAudioButtonBorderDip));
}

void QuickAnswersView::AddGoogleIcon() {
  // Add Google icon.
  auto* google_icon =
      main_view_->AddChildView(std::make_unique<views::ImageView>());
  google_icon->SetBorder(views::CreateEmptyBorder(kGoogleIconInsets));
  google_icon->SetImage(gfx::CreateVectorIcon(vector_icons::kGoogleColorIcon,
                                              kGoogleIconSizeDip,
                                              gfx::kPlaceholderColor));
}

int QuickAnswersView::GetBoundsWidth() {
  return anchor_view_bounds_.width();
}

int QuickAnswersView::GetLabelWidth() {
  return GetBoundsWidth() - kMainViewInsets.width() -
         kContentViewInsets.width() - kGoogleIconInsets.width() -
         kGoogleIconSizeDip;
}

void QuickAnswersView::ResetContentView() {
  content_view_->RemoveAllChildViews();
  first_answer_label_ = nullptr;
}

void QuickAnswersView::UpdateBounds() {
  // Multi-line labels need to be resized to be compatible with bounds width.
  if (first_answer_label_)
    first_answer_label_->SizeToFit(GetLabelWidth());

  int height = GetHeightForWidth(GetBoundsWidth());
  int y = anchor_view_bounds_.y() - kMarginDip - height;

  // Reserve space at the top since the view might expand for two-line answers.
  int y_min = anchor_view_bounds_.y() - kMarginDip - MaximumViewHeight();
  if (y_min < display::Screen::GetScreen()
                  ->GetDisplayMatching(anchor_view_bounds_)
                  .bounds()
                  .y()) {
    // The Quick Answers view will be off screen if showing above the anchor.
    // Show below the anchor instead.
    y = anchor_view_bounds_.bottom() + kMarginDip;
  }

  gfx::Rect bounds = {{anchor_view_bounds_.x(), y}, {GetBoundsWidth(), height}};
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For Ash, convert the position relative to the screen.
  // For Lacros, `bounds` is already relative to the toplevel window and the
  // position will be calculated on server side.
  wm::ConvertRectFromScreen(GetWidget()->GetNativeWindow()->parent(), &bounds);
#endif
  GetWidget()->SetBounds(bounds);
}

void QuickAnswersView::UpdateQuickAnswerResult(
    const QuickAnswer& quick_answer) {
  // Check if the view (or any of its children) had focus before resetting the
  // view, so it can be restored for the updated view.
  bool pane_already_had_focus = Contains(GetFocusManager()->GetFocusedView());
  ResetContentView();

  if (report_query_view_) {
    base_view_->RemoveChildViewT(report_query_view_.get());
    report_query_view_ = nullptr;
  }

  // Add title.
  View* title_view = AddHorizontalUiElements(quick_answer.title, content_view_);
  auto* title_label = static_cast<Label*>(title_view->children().front());
  title_label->SetMaximumWidthSingleLine(GetLabelWidth());

  // Add phonetics audio button for definition results.
  if (quick_answer.result_type == ResultType::kDefinitionResult &&
      (!quick_answer.phonetics_info.phonetics_audio.is_empty() ||
       quick_answer.phonetics_info.tts_audio_enabled))
    AddPhoneticsAudioButton(quick_answer.phonetics_info, title_view);

  // Add first row answer.
  View* first_answer_view = nullptr;
  if (!quick_answer.first_answer_row.empty()) {
    first_answer_view =
        AddHorizontalUiElements(quick_answer.first_answer_row, content_view_);
  }
  bool first_answer_is_single_label =
      first_answer_view->children().size() == 1 &&
      first_answer_view->children().front()->GetClassName() ==
          QuickAnswersTextLabel::kViewClassName;
  if (first_answer_is_single_label) {
    // Update announcement.
    auto* answer_label =
        static_cast<Label*>(first_answer_view->children().front());
    GetViewAccessibility().OverrideDescription(l10n_util::GetStringFUTF8(
        IDS_QUICK_ANSWERS_VIEW_A11Y_INFO_DESC_TEMPLATE_V2,
        title_label->GetText(), answer_label->GetText()));
  }

  // Add second row answer.
  if (!quick_answer.second_answer_row.empty()) {
    AddHorizontalUiElements(quick_answer.second_answer_row, content_view_);
  } else {
    // If secondary-answer does not exist and primary-answer is a single label,
    // allow that label to wrap through to the row intended for the former.
    if (first_answer_is_single_label) {
      // Cache multi-line label for resizing when view bounds change.
      first_answer_label_ = static_cast<QuickAnswersTextLabel*>(
          first_answer_view->children().front());
      first_answer_label_->SetMultiLine(true);
      first_answer_label_->SetMaxLines(kMaxRows - /*exclude title*/ 1);
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
    report_query_view_ = base_view_->AddChildView(
        std::make_unique<ReportQueryView>(base::BindRepeating(
            &QuickAnswersUiController::OnReportQueryButtonPressed,
            controller_)));
  }
}

std::vector<views::View*> QuickAnswersView::GetFocusableViews() {
  std::vector<views::View*> focusable_views;
  // The view itself does not gain focus for retry-view and transfers it to the
  // retry-label, and so is not included when this is the case.
  if (!retry_label_)
    focusable_views.push_back(this);
  if (dogfood_feedback_button_ && dogfood_feedback_button_->GetVisible())
    focusable_views.push_back(dogfood_feedback_button_);
  if (settings_button_ && settings_button_->GetVisible())
    focusable_views.push_back(settings_button_);
  if (phonetics_audio_button_ && phonetics_audio_button_->GetVisible())
    focusable_views.push_back(phonetics_audio_button_);
  if (retry_label_ && retry_label_->GetVisible())
    focusable_views.push_back(retry_label_);
  if (report_query_view_ && report_query_view_->GetVisible())
    focusable_views.push_back(report_query_view_);
  return focusable_views;
}

void QuickAnswersView::OnPhoneticsAudioButtonPressed(
    const PhoneticsInfo& phonetics_info) {
  // Use the phonetics audio URL if provided.
  if (!phonetics_info.phonetics_audio.is_empty()) {
    phonetics_audio_web_view_->LoadInitialURL(phonetics_info.phonetics_audio);
    return;
  }

  // Otherwise, generate and use tts audio.
  auto* tts_controller = content::TtsControllerImpl::GetInstance();
  std::unique_ptr<content::TtsUtterance> tts_utterance =
      content::TtsUtterance::Create(
          phonetics_audio_web_view_->GetBrowserContext());

  tts_controller->SetStopSpeakingWhenHidden(false);

  tts_utterance->SetShouldClearQueue(false);
  tts_utterance->SetText(phonetics_info.query_text);
  tts_utterance->SetLang(phonetics_info.locale);
  // TtsController will use the default TTS engine if the Google TTS engine
  // is not available.
  tts_utterance->SetEngineId(kGoogleTtsEngineId);
  tts_utterance->SetEventDelegate(new QuickAnswersUtteranceEventDelegate());

  tts_controller->SpeakOrEnqueue(std::move(tts_utterance));
}
