// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/projector/selfie_cam_bubble_manager.h"

#include <memory>

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/webui/chromeos/projector/projector_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/webview.h"

namespace chromeos {

namespace {

constexpr int kCornerRadiusDip = 80;

constexpr gfx::Size kPreferredSize(2 * kCornerRadiusDip, 2 * kCornerRadiusDip);

// Makes the selfie cam draggable.
class SelfieCamBubbleFrameView : public views::BubbleFrameView {
 public:
  SelfieCamBubbleFrameView()
      : views::BubbleFrameView(gfx::Insets(), gfx::Insets()) {
    auto border = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::FLOAT, views::BubbleBorder::DIALOG_SHADOW,
        gfx::kPlaceholderColor);
    // Needed to make the selfie cam round.
    border->SetCornerRadius(kCornerRadiusDip);
    views::BubbleFrameView::SetBubbleBorder(std::move(border));
  }

  ~SelfieCamBubbleFrameView() override = default;
  SelfieCamBubbleFrameView(const SelfieCamBubbleFrameView&) = delete;
  SelfieCamBubbleFrameView& operator=(const SelfieCamBubbleFrameView&) = delete;

  // Needed to make the selfie cam draggable everywhere within its bounds.
  int NonClientHitTest(const gfx::Point& point) override {
    // Outside of the window bounds, do nothing.
    if (!bounds().Contains(point))
      return HTNOWHERE;

    // Ensure it's within the BubbleFrameView. This takes into account the
    // rounded corners and drop shadow of the BubbleBorder.
    int hit = views::BubbleFrameView::NonClientHitTest(point);

    // After BubbleFrameView::NonClientHitTest processes the bubble-specific
    // hits such as the rounded corners, it checks hits to the bubble's client
    // view. Any hits to ClientFrameView::NonClientHitTest return HTCLIENT or
    // HTNOWHERE. Override these to return HTCAPTION in order to make the
    // entire widget draggable.
    return (hit == HTCLIENT || hit == HTNOWHERE) ? HTCAPTION : hit;
  }
};

// Dialog that displays the selfie cam for including the user's face in
// Projector screen recordings.
class SelfieCamBubbleDialogView : public WebUIBubbleDialogView {
 public:
  SelfieCamBubbleDialogView(
      std::unique_ptr<BubbleContentsWrapper> contents_wrapper)
      : WebUIBubbleDialogView(/*anchor_view=*/nullptr, contents_wrapper.get()),
        contents_wrapper_(std::move(contents_wrapper)) {
    set_has_parent(false);
    set_close_on_deactivate(false);
  }
  ~SelfieCamBubbleDialogView() override = default;

  // views::BubbleDialogDelegateView:
  // Opens the selfie cam in the middle of the screen initially.
  // TODO(crbug/1199396): Consider if the selfie cam should appear somewhere
  // else by default initially, such as the bottom right of the screen.
  gfx::Rect GetBubbleBounds() override {
    // Bubble bounds are what the computed bubble bounds would be, taking into
    // account the current bubble size.
    gfx::Rect bubble_bounds =
        views::BubbleDialogDelegateView::GetBubbleBounds();
    // Widget bounds are where the bubble currently is in space.
    gfx::Rect widget_bounds = GetWidget()->GetWindowBoundsInScreen();
    // Use the widget x and y to keep the bubble oriented at its current
    // location, and use the bubble width and height to set the correct bubble
    // size.
    return gfx::Rect(widget_bounds.x(), widget_bounds.y(),
                     bubble_bounds.width(), bubble_bounds.height());
  }

  // views::BubbleDialogDelegateView:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override {
    params->type = views::Widget::InitParams::TYPE_WINDOW;
    // Keeps the selfie cam always on top.
    params->z_order = ui::ZOrderLevel::kFloatingWindow;
    params->visible_on_all_workspaces = true;
  }

  // WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override {
    return std::make_unique<SelfieCamBubbleFrameView>();
  }

  // Disallows closing the selfie cam through pressing the escape key because
  // the toggle button gets out of sync with the model state. The only way to
  // close the selfie cam should be through the toggle off button.
  bool OnCloseRequested(views::Widget::ClosedReason close_reason) override {
    // Pressing escape maps to kCancelButtonClicked instead of kEscKeyPressed.
    return close_reason != views::Widget::ClosedReason::kCancelButtonClicked;
  }

  // views::View:
  gfx::Size CalculatePreferredSize() const override { return kPreferredSize; }

 private:
  std::unique_ptr<BubbleContentsWrapper> contents_wrapper_;
};

// Renders the WebUI contents and asks for camera permission so that
// we don't need to prompt the user.
class SelfieCamBubbleContentsWrapper
    : public BubbleContentsWrapperT<ProjectorUI> {
 public:
  SelfieCamBubbleContentsWrapper(const GURL& webui_url,
                                 content::BrowserContext* browser_context,
                                 int task_manager_string_id)
      : BubbleContentsWrapperT(webui_url,
                               browser_context,
                               task_manager_string_id) {}

  // content::WebContentsDelegate:
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override {
    MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
        web_contents, request, std::move(callback), /*extension=*/nullptr);
  }

  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override {
    return MediaCaptureDevicesDispatcher::GetInstance()
        ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
  }
};

}  // namespace

SelfieCamBubbleManager::SelfieCamBubbleManager() = default;
SelfieCamBubbleManager::~SelfieCamBubbleManager() = default;

void SelfieCamBubbleManager::Show(Profile* profile) {
  if (IsVisible())
    return;

  auto contents_wrapper = std::make_unique<SelfieCamBubbleContentsWrapper>(
      GURL(chrome::kChromeUIProjectorSelfieCamURL), profile,
      IDS_SELFIE_CAM_TITLE);
  // Need to reload the web contents here because the view isn't visible unless
  // ShowUI is called from the JS side.  By reloading, we trigger the JS to
  // eventually call ShowUI().
  contents_wrapper->ReloadWebContents();

  auto bubble_view =
      std::make_unique<SelfieCamBubbleDialogView>(std::move(contents_wrapper));

  bubble_view_ = bubble_view->GetWeakPtr();
  auto* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  // Needed to set the window.innerWidth and window.innerHeight to the preferred
  // size in JavaScript so we can determine the correct camera resolution.
  bubble_view_->web_view()->EnableSizingFromWebContents(
      /*min_size=*/kPreferredSize, /*max_size=*/kPreferredSize);
  // Needed to make the selfie cam round.
  bubble_view_->web_view()->holder()->SetCornerRadii(
      gfx::RoundedCornersF(kCornerRadiusDip));
  // Needed to make the selfie cam draggable everywhere within its bounds.
  bubble_view_->web_view()->holder()->SetHitTestTopInset(
      bubble_view_->height());
  bubble_widget->Show();
}

void SelfieCamBubbleManager::Close() {
  if (!IsVisible())
    return;

  DCHECK(bubble_view_->GetWidget());
  bubble_view_->GetWidget()->CloseNow();
}

bool SelfieCamBubbleManager::IsVisible() const {
  return bubble_view_ != nullptr;
}

}  // namespace chromeos
