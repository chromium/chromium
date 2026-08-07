// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/dictation_overlay_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/dictation/waveform_view.h"
#include "chrome/browser/ui/views/dictation/waveform_view_button.h"
#include "components/vector_icons/vector_icons.h"
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
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace dictation {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationOverlayView,
                                      kViewElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationOverlayView,
                                      kMicButtonElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationOverlayView,
                                      kWaveformElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationOverlayView,
                                      kFinalizingImageElementIdForTesting);

namespace {

constexpr int kCornerRadius = 16;

class DictationOverlayContentsView : public views::View {
  METADATA_HEADER(DictationOverlayContentsView, views::View)
 public:
  explicit DictationOverlayContentsView(
      base::RepeatingClosure toggle_active_stream_callback)
      : toggle_active_stream_callback_(
            std::move(toggle_active_stream_callback)) {
    SetProperty(views::kElementIdentifierKey,
                DictationOverlayView::kViewElementIdForTesting);

    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(6));
    SetLayoutManager(std::move(layout));

    // TODO(b/525859277): Use non-placeholder values.
    auto mic_button = views::CreateVectorImageButtonWithNativeTheme(
        toggle_active_stream_callback_, vector_icons::kMicIcon, 20,
        ui::kColorSysOnSurface, ui::kColorIconDisabled, ui::kColorSysOnSurface);
    mic_button->SetBorder(nullptr);
    mic_button->SetAccessibleName(u"Dictation");
    mic_button->SetPreferredSize(gfx::Size(20, 20));
    mic_button->SetProperty(
        views::kElementIdentifierKey,
        DictationOverlayView::kMicButtonElementIdForTesting);
    mic_button_ = AddChildView(std::move(mic_button));

    auto waveform_view = std::make_unique<WaveformViewButton>(
        /*full_size=*/false, toggle_active_stream_callback_);
    waveform_view->SetProperty(
        views::kElementIdentifierKey,
        DictationOverlayView::kWaveformElementIdForTesting);
    waveform_view->SetVisible(false);
    waveform_view_ = AddChildView(std::move(waveform_view));

    auto finalizing_image = std::make_unique<views::ImageView>();
    finalizing_image->SetImage(ui::ImageModel::FromVectorIcon(
        views::kMoreHorizIcon, ui::kColorSysOnSurface, 20));
    finalizing_image->SetPreferredSize(gfx::Size(20, 20));
    finalizing_image->SetProperty(
        views::kElementIdentifierKey,
        DictationOverlayView::kFinalizingImageElementIdForTesting);
    finalizing_image->SetVisible(false);
    finalizing_image_ = AddChildView(std::move(finalizing_image));
  }

  ~DictationOverlayContentsView() override = default;

  void SetState(UiState state) {
    if (state_ == state) {
      return;
    }
    state_ = state;

    bool mic_visible = false;
    bool waveform_visible = false;
    bool finalizing_dots_visible = false;
    switch (state) {
      case UiState::kInactive:
      case UiState::kInitializing:
        mic_visible = true;
        break;
      case UiState::kTranscribing:
        waveform_visible = true;
        break;
      case UiState::kFinalizing:
        finalizing_dots_visible = true;
        break;
    }

    mic_button_->SetVisible(mic_visible);

    waveform_view_->SetVisible(waveform_visible);
    waveform_view_->SetState(state);

    finalizing_image_->SetVisible(finalizing_dots_visible);

    PreferredSizeChanged();
  }

  void UpdateAudioLevel(float audio_level) {
    if (waveform_view_) {
      waveform_view_->SetAudioLevel(audio_level);
    }
  }

  UiState state() const { return state_; }

 private:
  base::RepeatingClosure toggle_active_stream_callback_;
  UiState state_ = UiState::kInactive;
  raw_ptr<views::ImageButton> mic_button_ = nullptr;
  raw_ptr<WaveformViewButton> waveform_view_ = nullptr;
  raw_ptr<views::ImageView> finalizing_image_ = nullptr;
};

BEGIN_METADATA(DictationOverlayContentsView)
END_METADATA

}  // namespace

DictationOverlayView::DictationOverlayView(
    gfx::NativeView parent_window,
    base::RepeatingClosure toggle_active_stream_callback)
    : BubbleDialogDelegate(nullptr,
                           views::BubbleBorder::TOP_LEFT,
                           views::BubbleBorder::STANDARD_SHADOW,
                           /*autosize=*/true) {
  set_parent_window(parent_window);
  SetBackgroundColor(ui::kColorBubbleBackground);
  SetContentsView(std::make_unique<DictationOverlayContentsView>(
      std::move(toggle_active_stream_callback)));
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(false);
  set_margins(gfx::Insets(0));
  set_corner_radius(kCornerRadius);
  set_shadow(views::BubbleBorder::STANDARD_SHADOW);
}

DictationOverlayView::~DictationOverlayView() = default;

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

  last_target_document_ = target_id.document;

  focus_selection_bounds_changed_subscription_ =
      web_contents->RegisterFocusSelectionBoundsChanged(base::BindRepeating(
          &DictationOverlayView::OnFocusSelectionBoundsChanged,
          base::Unretained(this)));

  UpdatePosition(target_rfh);
}

void DictationOverlayView::OnFocusSelectionBoundsChanged(
    content::RenderWidgetHostView* render_widget_host_view) {
  content::RenderFrameHost* target_rfh =
      last_target_document_.AsRenderFrameHostIfValid();
  if (!target_rfh || target_rfh->GetView() != render_widget_host_view) {
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

  std::optional<gfx::Point> point =
      web_contents->GetFocusSelectionPoint(target_rfh);
  if (!point.has_value()) {
    return;
  }

  if (widget_ && !web_contents->IsFocusedElementEditable()) {
    // If the user's selection changed to something that isn't editable, leave
    // the icon where it is. Since the last editable is where new text will go
    // for a new stream.
    return;
  }

  UpdatePosition(*point);
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
