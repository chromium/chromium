// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/dictation_overlay_view.h"

#include "base/functional/bind.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace dictation {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DictationOverlayView,
                                      kViewElementIdForTesting);

namespace {

constexpr int kCornerRadius = 12;

class DictationOverlayContentsView : public views::View {
  METADATA_HEADER(DictationOverlayContentsView, views::View)
 public:
  DictationOverlayContentsView() {
    SetProperty(views::kElementIdentifierKey,
                DictationOverlayView::kViewElementIdForTesting);

    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(2));
    SetLayoutManager(std::move(layout));

    // TODO(b/525859277): Use non-placeholder values.
    // TODO(b/525859277): Have clicks toggle the active stream.
    // TODO(b/525859277): Change the icon based on stream state.
    auto button = views::ImageButton::CreateIconButton(
        base::RepeatingClosure(base::DoNothing()), vector_icons::kMicIcon,
        u"Dictation");
    button->SetPreferredSize(gfx::Size(24, 24));
    AddChildView(std::move(button));
  }

  ~DictationOverlayContentsView() override = default;
};

BEGIN_METADATA(DictationOverlayContentsView)
END_METADATA

}  // namespace

DictationOverlayView::DictationOverlayView(gfx::NativeView parent_window)
    : BubbleDialogDelegate(nullptr,
                           views::BubbleBorder::TOP_LEFT,
                           views::BubbleBorder::STANDARD_SHADOW,
                           /*autosize=*/true) {
  set_parent_window(parent_window);
  SetContentsView(std::make_unique<DictationOverlayContentsView>());
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

}  // namespace dictation
