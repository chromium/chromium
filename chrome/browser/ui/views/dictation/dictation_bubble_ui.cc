// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/dictation/reviewing_page_status_view.h"
#include "chrome/browser/ui/views/dictation/waveform_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace dictation {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationBubbleUi,
                                      kViewElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationBubbleUi,
                                      kCloseButtonElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationBubbleUi,
                                      kToggleButtonElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationBubbleUi,
                                      kWaveformElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationBubbleUi,
                                      kSeparatorElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(
    DictationBubbleUi,
    kReviewingPageStatusLabelElementIdForTesting);

namespace {

// The contents view of the dictation toast.
class DictationToastView : public views::View {
  METADATA_HEADER(DictationToastView, views::View)
 public:
  explicit DictationToastView(
      base::RepeatingClosure close_callback,
      base::RepeatingClosure toggle_active_stream_callback,
      base::RepeatingClosure on_layout_changed_callback);
  ~DictationToastView() override;

  void Init();
  void ShowReviewingPageStatus();
  void UpdateForState(UiState state);
  void UpdateAudioLevel(float audio_level);
  void SetReviewingPageStatusDurationForTesting(base::TimeDelta duration);

  views::MdTextButton* toggle_button() { return toggle_button_; }
  views::ImageButton* close_button() { return close_button_; }

  // views::View:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 private:
  void OnReviewingPageStatusVisibilityChanged(bool visible);
  bool IsReviewingPageStatusVisible() const;

  base::RepeatingClosure close_callback_;
  base::RepeatingClosure toggle_active_stream_callback_;
  base::RepeatingClosure on_layout_changed_callback_;

  raw_ptr<views::ImageView> mic_view_ = nullptr;
  raw_ptr<WaveformView> waveform_view_ = nullptr;
  raw_ptr<ReviewingPageStatusView> status_view_ = nullptr;
  raw_ptr<views::MdTextButton> toggle_button_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;
};

}  // namespace

// --- DictationToastView ---

DictationToastView::DictationToastView(
    base::RepeatingClosure close_callback,
    base::RepeatingClosure toggle_active_stream_callback,
    base::RepeatingClosure on_layout_changed_callback)
    : close_callback_(std::move(close_callback)),
      toggle_active_stream_callback_(std::move(toggle_active_stream_callback)),
      on_layout_changed_callback_(std::move(on_layout_changed_callback)) {
  SetProperty(views::kElementIdentifierKey,
              DictationBubbleUi::kViewElementIdForTesting);
  SetNotifyEnterExitOnChild(true);
}

DictationToastView::~DictationToastView() = default;

void DictationToastView::Init() {
  ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // TODO(b/510778034): Determine what we need to make this accessibility
  // friendly.
  // TODO(b/510738735): Finalize placeholder strings.
  // TODO(b/512495405): Wrap the visual aspects of the view into a model so this
  // setup is common across elements..
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const gfx::VectorIcon& mic_icon_source = vector_icons::kMicDetectAutoIcon;
#else
  const gfx::VectorIcon& mic_icon_source = vector_icons::kMicIcon;
#endif

  views::ImageView* mic_icon =
      AddChildView(std::make_unique<views::ImageView>());
  mic_icon->SetImage(ui::ImageModel::FromVectorIcon(
      mic_icon_source, ui::kColorSysOnSurface,
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE)));

  const int child_spacing =
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING);
  const gfx::Insets child_margins = gfx::Insets::TLBR(0, child_spacing, 0, 0);

  WaveformView* waveform_view =
      AddChildView(std::make_unique<WaveformView>(/*full_size=*/true));
  waveform_view_ = waveform_view;
  waveform_view->SetProperty(views::kElementIdentifierKey,
                             DictationBubbleUi::kWaveformElementIdForTesting);
  waveform_view->SetProperty(views::kMarginsKey, child_margins);
  waveform_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));

  status_view_ = AddChildView(
      std::make_unique<ReviewingPageStatusView>(base::BindRepeating(
          &DictationToastView::OnReviewingPageStatusVisibilityChanged,
          base::Unretained(this))));
  status_view_->Init();
  status_view_->SetProperty(views::kMarginsKey, child_margins);

  views::MdTextButton* toggle_button =
      AddChildView(std::make_unique<views::MdTextButton>(
          toggle_active_stream_callback_, l10n_util::GetStringUTF16(IDS_DONE)));
  toggle_button_ = toggle_button;
  toggle_button->SetPreferredSize(gfx::Size(
      toggle_button->GetPreferredSize().width(),
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON)));
  toggle_button->SetStyle(ui::ButtonStyle::kProminent);
  toggle_button->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0,
          lp->GetDistanceMetric(
              DISTANCE_TOAST_BUBBLE_BETWEEN_LABEL_ACTION_BUTTON_SPACING),
          0, 0));
  toggle_button->SetProperty(
      views::kElementIdentifierKey,
      DictationBubbleUi::kToggleButtonElementIdForTesting);

  views::ImageButton* close_button =
      AddChildView(views::CreateVectorImageButtonWithNativeTheme(
          close_callback_,
          features::IsRoundedIconsEnabled()
              ? vector_icons::kCloseIcon
              : vector_icons::kCloseChromeRefreshOldIcon,
          lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE),
          ui::kColorSysOnSurface, ui::kColorIconDisabled,
          ui::kColorSysOnSurface));
  const gfx::Insets insets =
      lp->GetInsetsMetric(views::InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON);
  close_button->SetBorder(views::CreateEmptyBorder(insets));
  views::InstallCircleHighlightPathGenerator(close_button);
  close_button->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_CLOSE));
  close_button->SetProperty(views::kMarginsKey, child_margins);
  close_button->SetProperty(views::kElementIdentifierKey,
                            DictationBubbleUi::kCloseButtonElementIdForTesting);
  close_button_ = close_button;
}

void DictationToastView::UpdateForState(UiState state) {
  if (waveform_view_) {
    waveform_view_->SetState(state);
  }

  if (toggle_button_) {
    switch (state) {
      case UiState::kInactive:
        // Note that when `kSessionEndsOnStreamEnd` is enabled, the button does
        // not toggle streams, it can only end both the stream and session.
        // TODO(b/510738735): Finalize placeholder strings.
        toggle_button_->SetText(l10n_util::GetStringUTF16(
            kSessionEndsOnStreamEnd.Get() ? IDS_DONE
                                          : IDS_DICTATION_BUTTON_START));
        toggle_button_->SetEnabled(!kSessionEndsOnStreamEnd.Get());
        break;
      case UiState::kInitializing:
      case UiState::kTranscribing:
        toggle_button_->SetText(l10n_util::GetStringUTF16(IDS_DONE));
        toggle_button_->SetEnabled(true);
        break;
      case UiState::kFinalizing:
        toggle_button_->SetText(l10n_util::GetStringUTF16(IDS_DONE));
        toggle_button_->SetEnabled(false);
        break;
    }
  }
}

void DictationToastView::UpdateAudioLevel(float audio_level) {
  if (waveform_view_) {
    waveform_view_->SetAudioLevel(audio_level);
  }
}

void DictationToastView::ShowReviewingPageStatus() {
  if (status_view_) {
    status_view_->Show();
  }
}

void DictationToastView::SetReviewingPageStatusDurationForTesting(
    base::TimeDelta duration) {
  if (status_view_) {
    status_view_->SetDurationForTesting(duration);  // IN-TEST
  }
}

void DictationToastView::OnMouseEntered(const ui::MouseEvent& event) {
  if (status_view_) {
    status_view_->UpdateTimer();
  }
}

void DictationToastView::OnMouseExited(const ui::MouseEvent& event) {
  if (status_view_) {
    status_view_->UpdateTimer();
  }
}

void DictationToastView::OnReviewingPageStatusVisibilityChanged(bool visible) {
  if (waveform_view_) {
    waveform_view_->SetBarCount(visible
                                    ? WaveformView::kCompactFullSizeBarCount
                                    : WaveformView::kDefaultFullSizeBarCount);
  }
  if (toggle_button_) {
    toggle_button_->SetVisible(!visible);
  }
  if (on_layout_changed_callback_) {
    on_layout_changed_callback_.Run();
  }
}

bool DictationToastView::IsReviewingPageStatusVisible() const {
  return status_view_ && status_view_->GetVisible();
}

BEGIN_METADATA(DictationToastView)
END_METADATA

// --- DictationToastBubbleDelegate ---

DictationBubbleUi::DictationBubbleUi(
    views::View* anchor_view,
    base::RepeatingClosure close_callback,
    base::RepeatingClosure toggle_active_stream_callback,
    bool show_reviewing_page_status)
    : BubbleDialogDelegate(anchor_view, views::BubbleBorder::NONE) {
  SetBackgroundColor(ui::kColorBubbleBackground);
  SetShowCloseButton(false);
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_corner_radius(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_TOAST_BUBBLE_HEIGHT));
  set_close_on_deactivate(false);
  SetContentsView(std::make_unique<DictationToastView>(
      std::move(close_callback), std::move(toggle_active_stream_callback),
      base::BindRepeating(&DictationBubbleUi::SizeToContents,
                          base::Unretained(this))));

  // TODO(crbug.com/509983464): Update this to call an undeprecated factory
  // function when this bug is fixed.
  widget_ =
      base::WrapUnique(views::BubbleDialogDelegate::CreateBubbleDeprecated(
          this, views::Widget::InitParams::CLIENT_OWNS_WIDGET));

  GetBubbleFrameView()->bubble_border()->set_draw_border_stroke(false);

  if (show_reviewing_page_status) {
    views::AsViewClass<DictationToastView>(GetContentsView())
        ->ShowReviewingPageStatus();
  }
}

DictationBubbleUi::~DictationBubbleUi() = default;

void DictationBubbleUi::Show() {
  CHECK(widget_);
  widget_->ShowInactive();
  // While the reviewing page status is shown, `GetInitiallyFocusedView()`
  // returns the close button, which `ShowInactive()` stores as the view to
  // focus on activation. Clear it so that the widget becoming active without
  // explicit keyboard navigation doesn't focus the close button, which would
  // pause the reviewing page status timer.
  views::View* toggle_button =
      views::AsViewClass<DictationToastView>(GetContentsView())
          ->toggle_button();
  if (!toggle_button->GetVisible()) {
    widget_->GetFocusManager()->SetStoredFocusView(nullptr);
  }
}

void DictationBubbleUi::SetState(UiState state) {
  if (state_ == state) {
    return;
  }
  state_ = state;
  if (GetContentsView()) {
    views::AsViewClass<DictationToastView>(GetContentsView())
        ->UpdateForState(state);
  }
  if (GetWidget()) {
    SizeToContents();
  }
}

void DictationBubbleUi::UpdateAudioLevel(float audio_level) {
  if (GetContentsView()) {
    views::AsViewClass<DictationToastView>(GetContentsView())
        ->UpdateAudioLevel(audio_level);
  }
}

void DictationBubbleUi::SetReviewingPageStatusDurationForTesting(
    base::TimeDelta duration) {
  if (GetContentsView()) {
    views::AsViewClass<DictationToastView>(GetContentsView())
        ->SetReviewingPageStatusDurationForTesting(duration);  // IN-TEST
  }
}

void DictationBubbleUi::Init() {
  CHECK(GetContentsView());
  auto* toast_view = views::AsViewClass<DictationToastView>(GetContentsView());
  toast_view->Init();
  toast_view->UpdateForState(state_);

  SetInitiallyFocusedView(toast_view->toggle_button());

  const auto* const layout_provider = ChromeLayoutProvider::Get();
  const gfx::Insets insets = layout_provider->GetInsetsMetric(
      views::InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON);
  const int max_child_height = std::max(
      {layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT),
       layout_provider->GetDistanceMetric(
           DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON),
       layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE) +
           insets.height()});

  const int total_vertical_margins =
      layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT) -
      max_child_height;
  const int top_margin = total_vertical_margins / 2;

  set_margins(gfx::Insets::TLBR(
      top_margin,
      layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_LEFT),
      total_vertical_margins - top_margin,
      layout_provider->GetDistanceMetric(
          DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_CLOSE_BUTTON)));
}

views::View* DictationBubbleUi::GetInitiallyFocusedView() {
  auto* toast_view = views::AsViewClass<DictationToastView>(GetContentsView());
  if (!toast_view) {
    return nullptr;
  }
  // The toggle button is hidden while the reviewing page status is shown. Fall
  // back to the close button so that keyboard users can still move focus into
  // the bubble (e.g. via pane cycling), since a hidden view can't take focus.
  views::View* toggle_button = toast_view->toggle_button();
  if (toggle_button && toggle_button->GetVisible()) {
    return toggle_button;
  }
  return toast_view->close_button();
}

gfx::Rect DictationBubbleUi::GetBubbleBounds() {
  views::View* anchor_view = GetAnchorView();
  if (!anchor_view || !GetWidget()) {
    return gfx::Rect();
  }

  const gfx::Size preferred_size =
      GetWidget()->GetContentsView()->GetPreferredSize();
  const gfx::Rect anchor_bounds = anchor_view->GetBoundsInScreen();

  const int minimum_margin = ChromeLayoutProvider::Get()->GetDistanceMetric(
                                 DISTANCE_TOAST_BUBBLE_BROWSER_WINDOW_MARGIN) -
                             views::BubbleBorder::kShadowBlur;
  const int width =
      std::min(preferred_size.width(),
               std::max(anchor_bounds.width() - 2 * minimum_margin, 0));
  const int x = anchor_bounds.x() + ((anchor_bounds.width() - width) / 2);

  // Overlap the bottom of the toolbar/omnibox by only a few pixels (e.g. 10
  // dip).
  constexpr int kOverlapAmount = 10;
  const int y = anchor_bounds.bottom() - kOverlapAmount;
  return gfx::Rect(x, y, width, preferred_size.height());
}

}  // namespace dictation
