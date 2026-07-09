// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/dictation/waveform_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/grit/branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
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

namespace {

// The contents view of the dictation toast.
class DictationToastView : public views::View {
  METADATA_HEADER(DictationToastView, views::View)
 public:
  explicit DictationToastView(
      base::RepeatingClosure close_callback,
      base::RepeatingClosure toggle_active_stream_callback);
  ~DictationToastView() override;

  void Init();
  void UpdateForState(DictationBubbleUi::State state);

 private:
  base::RepeatingClosure close_callback_;
  base::RepeatingClosure toggle_active_stream_callback_;
  raw_ptr<views::ImageView> mic_view_ = nullptr;
  raw_ptr<WaveformView> waveform_view_ = nullptr;
  raw_ptr<views::MdTextButton> toggle_button_ = nullptr;
};

}  // namespace

// --- DictationToastView ---

DictationToastView::DictationToastView(
    base::RepeatingClosure close_callback,
    base::RepeatingClosure toggle_active_stream_callback)
    : close_callback_(std::move(close_callback)),
      toggle_active_stream_callback_(std::move(toggle_active_stream_callback)) {
  SetProperty(views::kElementIdentifierKey,
              DictationBubbleUi::kViewElementIdForTesting);
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
  views::ImageView* mic_icon =
      AddChildView(std::make_unique<views::ImageView>());
  mic_icon->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kMicIcon, ui::kColorSysOnSurface,
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE)));

  WaveformView* waveform_view = AddChildView(std::make_unique<WaveformView>());
  waveform_view_ = waveform_view;
  waveform_view->SetProperty(views::kElementIdentifierKey,
                             DictationBubbleUi::kWaveformElementIdForTesting);
  waveform_view->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING),
          0, 0));
  waveform_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));

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
  close_button->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(
          0, lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING),
          0, 0));
  close_button->SetProperty(views::kElementIdentifierKey,
                            DictationBubbleUi::kCloseButtonElementIdForTesting);
}

void DictationToastView::UpdateForState(DictationBubbleUi::State state) {
  if (waveform_view_) {
    waveform_view_->SetState(state);
  }

  if (toggle_button_) {
    switch (state) {
      case DictationBubbleUi::State::kInactive:
        // TODO(b/510738735): Finalize placeholder strings.
        toggle_button_->SetText(
            l10n_util::GetStringUTF16(IDS_DICTATION_BUTTON_START));
        toggle_button_->SetEnabled(true);
        break;
      case DictationBubbleUi::State::kInitializing:
      case DictationBubbleUi::State::kTranscribing:
        toggle_button_->SetText(l10n_util::GetStringUTF16(IDS_DONE));
        toggle_button_->SetEnabled(true);
        break;
      case DictationBubbleUi::State::kFinalizing:
        toggle_button_->SetText(l10n_util::GetStringUTF16(IDS_DONE));
        toggle_button_->SetEnabled(false);
        break;
    }
  }
}

BEGIN_METADATA(DictationToastView)
END_METADATA

// --- DictationToastBubbleDelegate ---

DictationBubbleUi::DictationBubbleUi(
    views::View* anchor_view,
    base::RepeatingClosure close_callback,
    base::RepeatingClosure toggle_active_stream_callback)
    : BubbleDialogDelegate(anchor_view, views::BubbleBorder::NONE) {
  SetBackgroundColor(ui::kColorBubbleBackground);
  SetShowCloseButton(false);
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_corner_radius(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_TOAST_BUBBLE_HEIGHT));
  set_close_on_deactivate(false);
  SetContentsView(std::make_unique<DictationToastView>(
      std::move(close_callback), std::move(toggle_active_stream_callback)));
  SetCanActivate(false);

  // TODO(crbug.com/509983464): Update this to call an undeprecated factory
  // function when this bug is fixed.
  widget_ =
      base::WrapUnique(views::BubbleDialogDelegate::CreateBubbleDeprecated(
          this, views::Widget::InitParams::CLIENT_OWNS_WIDGET));

  GetBubbleFrameView()->bubble_border()->set_draw_border_stroke(false);
}

DictationBubbleUi::~DictationBubbleUi() = default;

void DictationBubbleUi::Show() {
  CHECK(widget_);
  widget_->ShowInactive();
}

void DictationBubbleUi::SetState(State state) {
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

void DictationBubbleUi::Init() {
  CHECK(GetContentsView());
  auto* toast_view = views::AsViewClass<DictationToastView>(GetContentsView());
  toast_view->Init();
  toast_view->UpdateForState(state_);

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
