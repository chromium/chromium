// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"

#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/visibility.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// The min size available to the WebBubbleDialogView. These are arbitrary sizes
// that match those set by ExtensionPopup.
constexpr gfx::Size kMinSize(25, 25);

// WebUIBubbleView provides the functionality needed to embed a WebContents
// within a Views hierarchy.
class WebUIBubbleView : public views::WebView {
 public:
  explicit WebUIBubbleView(content::WebContents* web_contents) {
    SetWebContents(web_contents);
    // Allow the embedder to handle accelerators not handled by the WebContents.
    set_allow_accelerators(true);
    SetPreferredSize(web_contents->GetSize());
  }
  ~WebUIBubbleView() override = default;

  // WebView:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override {
    // Ignores context menu.
    return true;
  }
};

}  // namespace

WebUIBubbleDialogView::WebUIBubbleDialogView(
    views::View* anchor_view,
    BubbleContentsWrapper* contents_wrapper,
    const absl::optional<gfx::Rect>& anchor_rect,
    views::BubbleBorder::Arrow arrow)
    : BubbleDialogDelegateView(anchor_view, arrow),
      contents_wrapper_(contents_wrapper),
      web_view_(AddChildView(std::make_unique<WebUIBubbleView>(
          contents_wrapper_->web_contents()))),
      bubble_anchor_(anchor_rect) {
  DCHECK(!contents_wrapper_->GetHost());
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());

  // Ensure the WebContents is in a visible state after being added to the
  // Views bubble so the correct lifecycle hooks are triggered.
  DCHECK_NE(content::Visibility::VISIBLE,
            contents_wrapper_->web_contents()->GetVisibility());
  contents_wrapper_->web_contents()->WasShown();

  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_margins(gfx::Insets());
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

WebUIBubbleDialogView::~WebUIBubbleDialogView() {
  ClearContentsWrapper();
}

void WebUIBubbleDialogView::ClearContentsWrapper() {
  if (!contents_wrapper_)
    return;
  DCHECK_EQ(this, contents_wrapper_->GetHost().get());
  DCHECK_EQ(web_view_->web_contents(), contents_wrapper_->web_contents());
  web_view_->SetWebContents(nullptr);
  contents_wrapper_->web_contents()->WasHidden();
  contents_wrapper_->SetHost(nullptr);
  contents_wrapper_ = nullptr;
}

base::WeakPtr<WebUIBubbleDialogView> WebUIBubbleDialogView::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void WebUIBubbleDialogView::OnWidgetClosing(views::Widget* widget) {
  ClearContentsWrapper();
}

gfx::Size WebUIBubbleDialogView::CalculatePreferredSize() const {
  // Constrain the size to popup min/max.
  gfx::Size preferred_size = BubbleDialogDelegateView::CalculatePreferredSize();
  preferred_size.SetToMax(kMinSize);
  preferred_size.SetToMin(GetWidget()->GetWorkAreaBoundsInScreen().size());
  return preferred_size;
}

void WebUIBubbleDialogView::AddedToWidget() {
  BubbleDialogDelegateView::AddedToWidget();
  bubble_widget_observation_.Observe(GetWidget());
  web_view_->holder()->SetCornerRadii(gfx::RoundedCornersF(GetCornerRadius()));
}

void WebUIBubbleDialogView::ShowUI() {
  DCHECK(GetWidget());
  GetWidget()->Show();
  web_view_->GetWebContents()->Focus();
}

void WebUIBubbleDialogView::CloseUI() {
  DCHECK(GetWidget());
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void WebUIBubbleDialogView::ResizeDueToAutoResize(content::WebContents* source,
                                                  const gfx::Size& new_size) {
  web_view_->SetPreferredSize(new_size);
  SizeToContents();
}

bool WebUIBubbleDialogView::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

gfx::Rect WebUIBubbleDialogView::GetAnchorRect() const {
  if (bubble_anchor_)
    return bubble_anchor_.value();
  return BubbleDialogDelegateView::GetAnchorRect();
}

BEGIN_METADATA(WebUIBubbleDialogView, views::BubbleDialogDelegateView)
END_METADATA
