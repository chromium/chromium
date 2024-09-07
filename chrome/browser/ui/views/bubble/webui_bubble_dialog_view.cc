// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"

#include "build/build_config.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/visibility.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/views/bubble/webui_bubble_event_handler_aura.h"
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

namespace {

// The min size available to the WebBubbleDialogView. These are arbitrary sizes
// that match those set by ExtensionPopup.
constexpr gfx::Size kMinSize(25, 25);

#if defined(USE_AURA)
bool ShouldUseEventHandlerForBubbleDrag(aura::Window* window) {
#if BUILDFLAG(ENABLE_DESKTOP_AURA)
  // Only use the event handler for non desktop aura windows. In the case of
  // desktop aura windows the host WM is responsible for controlling the drag.
  return views::DesktopNativeWidgetAura::ForWindow(window) == nullptr;
#else
  return true;
#endif
}
#endif

// WebUIBubbleView provides the functionality needed to embed a WebContents
// within a Views hierarchy.
class WebUIBubbleView : public views::WebView {
  METADATA_HEADER(WebUIBubbleView, views::WebView)

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

SkRegion ComputeDraggableRegion(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions) {
  SkRegion draggable_region;
  for (const blink::mojom::DraggableRegionPtr& region : regions) {
    draggable_region.op(
        SkIRect::MakeXYWH(region->bounds.x(), region->bounds.y(),
                          region->bounds.width(), region->bounds.height()),
        region->draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
  }
  return draggable_region;
}

BEGIN_METADATA(WebUIBubbleView)
END_METADATA

}  // namespace

WebUIBubbleDialogView::WebUIBubbleDialogView(
    views::View* anchor_view,
    base::WeakPtr<WebUIContentsWrapper> contents_wrapper,
    const std::optional<gfx::Rect>& anchor_rect,
    views::BubbleBorder::Arrow arrow,
    bool autosize)
    : BubbleDialogDelegateView(anchor_view,
                               arrow,
                               views::BubbleBorder::DIALOG_SHADOW,
                               autosize),
      contents_wrapper_(contents_wrapper),
      web_view_(AddChildView(std::make_unique<WebUIBubbleView>(
          contents_wrapper_->web_contents()))),
      bubble_anchor_(anchor_rect) {
  DCHECK(!contents_wrapper_->GetHost());

  contents_wrapper_->web_contents()->WasShown();

  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
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

gfx::Size WebUIBubbleDialogView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // Constrain the size to popup min/max.
  gfx::Size preferred_size =
      BubbleDialogDelegateView::CalculatePreferredSize(available_size);
  preferred_size.SetToMax(kMinSize);
  preferred_size.SetToMin(GetWidget()->GetWorkAreaBoundsInScreen().size());
  return preferred_size;
}

void WebUIBubbleDialogView::AddedToWidget() {
  BubbleDialogDelegateView::AddedToWidget();
  // This view needs to be added to the widget before setting itself as the host
  // of the contents, so that the contents' resizing request can be propagated
  // to the widget.
  views::Widget* widget = GetWidget();
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
  bubble_widget_observation_.Observe(widget);
  web_view_->holder()->SetCornerRadii(gfx::RoundedCornersF(GetCornerRadius()));

#if defined(USE_AURA)
  aura::Window* window = widget->GetNativeView();
  if (ShouldUseEventHandlerForBubbleDrag(window)) {
    event_handler_ = std::make_unique<WebUIBubbleEventHandlerAura>();
    window->AddPreTargetHandler(event_handler_.get());
  }
#endif
}

gfx::Rect WebUIBubbleDialogView::GetBubbleBounds() {
  // If hosting a draggable bubble do not update widget bounds position while
  // the bubble is visible. This allows the bubble to position itself according
  // to the anchor initially while retaining its dragged position as the bubble
  // resizes.
  gfx::Rect bubble_bounds = BubbleDialogDelegateView::GetBubbleBounds();
  const views::Widget* widget = GetWidget();
  if (contents_wrapper_ && contents_wrapper_->supports_draggable_regions() &&
      widget && widget->IsVisible()) {
    bubble_bounds.set_origin(widget->GetWindowBoundsInScreen().origin());
  }
  return bubble_bounds;
}

std::unique_ptr<views::NonClientFrameView>
WebUIBubbleDialogView::CreateNonClientFrameView(views::Widget* widget) {
  // TODO(tluk): Improve the current pattern used to compose functionality on
  // bubble frames and eliminate the need for static cast.
  auto frame = BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->set_non_client_hit_test_cb(base::BindRepeating(
          &WebUIBubbleDialogView::NonClientHitTest, base::Unretained(this)));
  return frame;
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
}

bool WebUIBubbleDialogView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

void WebUIBubbleDialogView::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* contents) {
  draggable_region_ = ComputeDraggableRegion(regions);
}

bool WebUIBubbleDialogView::ShouldDescendIntoChildForEventHandling(
    gfx::NativeView child,
    const gfx::Point& location) {
  // The bubble should claim events that fall within the draggable region.
  return !draggable_region_.has_value() ||
         !draggable_region_->contains(location.x(), location.y());
}

gfx::Rect WebUIBubbleDialogView::GetAnchorRect() const {
  if (bubble_anchor_)
    return bubble_anchor_.value();
  return BubbleDialogDelegateView::GetAnchorRect();
}

int WebUIBubbleDialogView::NonClientHitTest(const gfx::Point& point) const {
  // Convert the point to the WebView's coordinates.
  gfx::Point point_in_webview =
      views::View::ConvertPointToTarget(this, web_view_, point);
  return draggable_region_.has_value() &&
                 draggable_region_->contains(point_in_webview.x(),
                                             point_in_webview.y())
             ? HTCAPTION
             : HTNOWHERE;
}

BEGIN_METADATA(WebUIBubbleDialogView)
END_METADATA
