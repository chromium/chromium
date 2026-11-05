// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/dictation_overlay_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/views/dictation/waveform_view_button.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_variant.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace dictation {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationOverlayView,
                                      kViewElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationOverlayView,
                                      kWaveformElementIdForTesting);

namespace {

constexpr int kCornerRadius = 16;
constexpr int kTeardropCornerRadius = 4;

FullscreenController* GetFullscreenController(
    content::WebContents* web_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return nullptr;
  }
  ExclusiveAccessManager* exclusive_access_manager =
      ExclusiveAccessManager::From(tab->GetBrowserWindowInterface());
  return exclusive_access_manager
             ? exclusive_access_manager->fullscreen_controller()
             : nullptr;
}

class DictationOverlayContentsView : public views::View {
  METADATA_HEADER(DictationOverlayContentsView, views::View)

 public:
  explicit DictationOverlayContentsView(
      base::RepeatingClosure end_active_stream_callback) {
    SetProperty(views::kElementIdentifierKey,
                DictationOverlayView::kViewElementIdForTesting);

    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(6));
    SetLayoutManager(std::move(layout));

    auto waveform_view = std::make_unique<WaveformViewButton>(
        /*full_size=*/false, std::move(end_active_stream_callback));
    waveform_view->SetProperty(
        views::kElementIdentifierKey,
        DictationOverlayView::kWaveformElementIdForTesting);
    waveform_view->SetEnabled(false);
    waveform_view_ = AddChildView(std::move(waveform_view));
  }

  ~DictationOverlayContentsView() override = default;

  void SetState(UiState state) {
    if (state_ == state) {
      return;
    }
    state_ = state;

    waveform_view_->SetMode(ToWaveformMode(state));
    waveform_view_->SetEnabled(state == UiState::kInitializing ||
                               state == UiState::kTranscribing);

    PreferredSizeChanged();
  }

  void UpdateAudioLevel(float audio_level) {
    if (waveform_view_) {
      waveform_view_->SetAudioLevel(audio_level);
    }
  }

  UiState state() const { return state_; }

 private:
  UiState state_ = UiState::kInactive;
  raw_ptr<WaveformViewButton> waveform_view_ = nullptr;
};

BEGIN_METADATA(DictationOverlayContentsView)
END_METADATA

}  // namespace

DictationOverlayView::DictationOverlayView(
    gfx::NativeView parent_window,
    base::RepeatingClosure end_active_stream_callback)
    : BubbleDialogDelegate(nullptr,
                           views::BubbleBorder::TOP_LEFT,
                           views::BubbleBorder::STANDARD_SHADOW,
                           /*autosize=*/true) {
  set_parent_window(parent_window);
  SetBackgroundColor(ui::kColorBubbleBackground);
  SetContentsView(std::make_unique<DictationOverlayContentsView>(
      std::move(end_active_stream_callback)));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(false);
  set_margins(gfx::Insets(0));
  set_corner_radius(kCornerRadius);
  set_shadow_config({.shadow_type = views::BubbleBorder::STANDARD_SHADOW});
}

DictationOverlayView::~DictationOverlayView() = default;

void DictationOverlayView::OnWidgetInitialized() {
  views::BubbleDialogDelegate::OnWidgetInitialized();
  if (GetBubbleFrameView()) {
    GetBubbleFrameView()->SetRoundedCorners(
        base::i18n::IsRTL()
            ? gfx::RoundedCornersF(kCornerRadius, kTeardropCornerRadius,
                                   kCornerRadius, kCornerRadius)
            : gfx::RoundedCornersF(kTeardropCornerRadius, kCornerRadius,
                                   kCornerRadius, kCornerRadius));
  }
}

void DictationOverlayView::Show() {
  if (!widget_) {
    widget_ = views::BubbleDialogDelegate::CreateBubble(this);
  }
  widget_->ShowInactive();
}

// TODO(b/525859277): Make sure this works for RTL text.
void DictationOverlayView::UpdatePosition(
    const gfx::Point& focus_selection_point) {
  SetAnchorRect(gfx::Rect(focus_selection_point, gfx::Size()));
  SizeToContents();
}

void DictationOverlayView::OnStartedStream(content::GlobalDOMNodeId target_id) {
  focus_selection_bounds_changed_subscription_ = {};
  fullscreen_subscription_ = {};

  content::RenderFrameHost* target_rfh =
      target_id.document.AsRenderFrameHostIfValid();
  if (!target_rfh) {
    return;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(target_rfh);
  if (!web_contents) {
    return;
  }

  last_target_node_id_ = target_id;

  focus_selection_bounds_changed_subscription_ =
      web_contents->RegisterFocusSelectionBoundsChanged(base::BindRepeating(
          &DictationOverlayView::OnFocusSelectionBoundsChanged,
          base::Unretained(this)));

  if (FullscreenController* fullscreen_controller =
          GetFullscreenController(web_contents)) {
    fullscreen_subscription_ =
        fullscreen_controller->RegisterOnFullscreenStateChanged(
            base::BindRepeating(&DictationOverlayView::OnFullscreenStateChanged,
                                base::Unretained(this)));
  }

  UpdatePosition(target_rfh);
}

void DictationOverlayView::OnFocusSelectionBoundsChanged(
    content::RenderWidgetHostView* render_widget_host_view) {
  content::RenderFrameHost* target_rfh =
      last_target_node_id_.document.AsRenderFrameHostIfValid();
  if (!target_rfh || target_rfh->GetView() != render_widget_host_view) {
    return;
  }

  UpdatePosition(target_rfh);
}

void DictationOverlayView::OnFullscreenStateChanged() {
  content::RenderFrameHost* target_rfh =
      last_target_node_id_.document.AsRenderFrameHostIfValid();
  if (!target_rfh) {
    return;
  }

  UpdatePosition(target_rfh);
}

void DictationOverlayView::UpdatePosition(
    content::RenderFrameHost* target_rfh) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(target_rfh);
  if (!web_contents) {
    return;
  }

  std::optional<gfx::Rect> bounds =
      web_contents->GetFocusSelectionBounds(target_rfh);
  if (!bounds.has_value()) {
    return;
  }

  if (widget_ && (web_contents->GetFocusedFrame() != target_rfh ||
                  target_rfh->GetFocusedDOMNodeId() !=
                      last_target_node_id_.target_element_dom_id)) {
    // If the targeted editable node lost focus, leave the icon where it is, as
    // that's where the text is going to be committed.
    return;
  }

  gfx::Point point = bounds->origin() + gfx::Vector2d(bounds->width(), 0);
  UpdatePosition(point);
  Show();
}

void DictationOverlayView::SetState(UiState state) {
  if (state_ == state) {
    return;
  }
  state_ = state;
  if (GetContentsView()) {
    views::AsViewClass<DictationOverlayContentsView>(GetContentsView())
        ->SetState(state);
  }
  if (GetWidget()) {
    SizeToContents();
  }
}

void DictationOverlayView::UpdateAudioLevel(float audio_level) {
  if (GetContentsView()) {
    views::AsViewClass<DictationOverlayContentsView>(GetContentsView())
        ->UpdateAudioLevel(audio_level);
  }
}

UiState DictationOverlayView::state_for_testing() const {
  return state_;
}

}  // namespace dictation
