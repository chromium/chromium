// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/projector/selfie_cam_bubble_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "ash/utility/rounded_window_targeter.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "ash/webui/projector_app/trusted_projector_ui.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"

namespace chromeos {

namespace {

constexpr int kCornerRadiusDipSmall = 80;
constexpr int kCornerRadiusDipLarge = 160;

// This is the minimum size such that the text on the expand/collapse button
// still appears.
constexpr int kExpandCollapseButtonRadius = 23;
constexpr gfx::Size kExpandCollapseButtonPreferredSize(
    2 * kExpandCollapseButtonRadius,
    2 * kExpandCollapseButtonRadius);
constexpr int kButtonCircleHighlightPaddingDip = 2;

// Margin of the bubble with respect to the context window.
constexpr int kMinAnchorMarginDip = 15;

std::unique_ptr<views::MdTextButton> BuildButton(
    views::Button::PressedCallback callback,
    const int tooltip_text_id,
    bool is_visible) {
  std::u16string text = l10n_util::GetStringUTF16(tooltip_text_id);
  auto button =
      std::make_unique<views::MdTextButton>(std::move(callback), text);
  button->SetProminent(true);
  button->SetTooltipText(text);
  button->SetCornerRadius(kExpandCollapseButtonRadius);
  button->SetPreferredSize(kExpandCollapseButtonPreferredSize);
  button->SetVisible(is_visible);
  views::InstallCircleHighlightPathGenerator(
      button.get(), gfx::Insets(kButtonCircleHighlightPaddingDip));
  return button;
}

// Makes the selfie cam draggable.
class SelfieCamBubbleFrameView : public views::BubbleFrameView {
 public:
  explicit SelfieCamBubbleFrameView(std::vector<views::View*> buttons)
      : views::BubbleFrameView(gfx::Insets(), gfx::Insets()),
        buttons_(std::move(buttons)) {
    auto border = std::make_unique<views::BubbleBorder>(
        views::BubbleBorder::FLOAT, views::BubbleBorder::DIALOG_SHADOW,
        gfx::kPlaceholderColor);
    // Needed to make the selfie cam round.
    border->SetCornerRadius(kCornerRadiusDipLarge);
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

    gfx::Point point_in_screen =
        GetBoundsInScreen().origin() + gfx::Vector2d(point.x(), point.y());
    for (views::View* button : buttons_) {
      if (button->GetBoundsInScreen().Contains(point_in_screen))
        return HTCLIENT;
    }

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

 private:
  std::vector<views::View*> buttons_;
};

// Dialog that displays the selfie cam for including the user's face in
// Projector screen recordings.
class SelfieCamBubbleDialogView : public WebUIBubbleDialogView {
 public:
  SelfieCamBubbleDialogView(
      std::unique_ptr<BubbleContentsWrapper> contents_wrapper,
      const gfx::Rect& context_bounds_in_screen)
      : WebUIBubbleDialogView(/*anchor_view=*/nullptr, contents_wrapper.get()),
        contents_wrapper_(std::move(contents_wrapper)),
        context_bounds_in_screen_(context_bounds_in_screen) {
    set_has_parent(false);
    set_close_on_deactivate(false);
  }
  ~SelfieCamBubbleDialogView() override = default;

  // views::BubbleDialogDelegateView:
  // Opens the selfie cam in the bottom-right of the `context_bounds_in_screen_`
  // rectangle initially.
  gfx::Rect GetBubbleBounds() override {
    // Bubble bounds are what the computed bubble bounds would be, taking into
    // account the current bubble size.
    gfx::Rect bubble_bounds =
        views::BubbleDialogDelegateView::GetBubbleBounds();

    gfx::Rect context_rect = context_bounds_in_screen_;
    context_rect.Inset(gfx::Insets(kMinAnchorMarginDip));
    int target_x = context_rect.right() - bubble_bounds.width();
    int target_y = context_rect.bottom() - bubble_bounds.height();
    return gfx::Rect(target_x, target_y, bubble_bounds.width(),
                     bubble_bounds.height());
  }

  // views::BubbleDialogDelegateView:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override {
    params->type = views::Widget::InitParams::TYPE_WINDOW;
    // Keeps the selfie cam always on top.
    params->z_order = ui::ZOrderLevel::kFloatingWindow;
    params->visible_on_all_workspaces = true;
  }

  void Init() override {
    auto content_container = std::make_unique<views::View>();
    content_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kVertical)
        .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetInteriorMargin(gfx::Insets::TLBR(0, 0, 8, 0));

    views::Button::PressedCallback expand_or_collapse_callback =
        base::BindRepeating(
            &SelfieCamBubbleDialogView::ExpandOrCollapseButtonPressed,
            base::Unretained(this));
    auto expand_button = BuildButton(expand_or_collapse_callback,
                                     IDS_SELFIE_CAMERA_BUBBLE_EXPANDED,
                                     /*is_visible=*/is_expanded_);

    auto collapse_button = BuildButton(std::move(expand_or_collapse_callback),
                                       IDS_SELFIE_CAMERA_BUBBLE_COLLAPSED,
                                       /*is_visible=*/!is_expanded_);

    expand_button_ = content_container->AddChildView(std::move(expand_button));
    collapse_button_ =
        content_container->AddChildView(std::move(collapse_button));
    AddChildView(std::move(content_container));
  }

  // WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override {
    std::vector<views::View*> buttons = {expand_button_, collapse_button_};
    return std::make_unique<SelfieCamBubbleFrameView>(std::move(buttons));
  }

  // Disallows closing the selfie cam through pressing the escape key because
  // the toggle button gets out of sync with the model state. The only way to
  // close the selfie cam should be through the toggle off button.
  bool OnCloseRequested(views::Widget::ClosedReason close_reason) override {
    // Pressing escape maps to kCancelButtonClicked instead of kEscKeyPressed.
    return close_reason != views::Widget::ClosedReason::kCancelButtonClicked;
  }

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(2 * current_radius_, 2 * current_radius_);
  }

  void Redraw() override {
    gfx::Size preferred_size = CalculatePreferredSize();
    // This view should be a circle with equal width and height.
    DCHECK_EQ(preferred_size.width(), preferred_size.height());
    int diameter = preferred_size.height();

    web_view()->holder()->SetHitTestTopInset(diameter);

    // Needed to set the window.innerWidth and window.innerHeight to the
    // preferred size in JavaScript so we can determine the correct camera
    // resolution.
    web_view()->EnableSizingFromWebContents(
        /*min_size=*/preferred_size,
        /*max_size=*/preferred_size);

    GetWidget()->GetNativeWindow()->SetEventTargeter(
        std::make_unique<ash::RoundedWindowTargeter>(current_radius_));
  }

 private:
  void ExpandOrCollapseButtonPressed() {
    is_expanded_ = !is_expanded_;
    expand_button_->SetVisible(is_expanded_);
    collapse_button_->SetVisible(!is_expanded_);

    current_radius_ =
        is_expanded_ ? kCornerRadiusDipLarge : kCornerRadiusDipSmall;

    Redraw();
  }

  std::unique_ptr<BubbleContentsWrapper> contents_wrapper_;
  const gfx::Rect context_bounds_in_screen_;

  views::MdTextButton* expand_button_ = nullptr;
  views::MdTextButton* collapse_button_ = nullptr;
  // Whether the selfie cam is in large or small mode.
  bool is_expanded_ = true;

  int current_radius_ = kCornerRadiusDipLarge;
};

// Renders the WebUI contents and asks for camera permission so that
// we don't need to prompt the user.
class SelfieCamBubbleContentsWrapper
    : public BubbleContentsWrapperT<ash::TrustedProjectorUI> {
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

void SelfieCamBubbleManager::Show(Profile* profile,
                                  const gfx::Rect& context_bounds_in_screen) {
  if (IsVisible())
    return;

  auto contents_wrapper = std::make_unique<SelfieCamBubbleContentsWrapper>(
      GURL(ash::kChromeUITrustedProjectorSelfieCamUrl), profile,
      IDS_SELFIE_CAM_TITLE);
  // Need to reload the web contents here because the view isn't visible unless
  // ShowUI is called from the JS side.  By reloading, we trigger the JS to
  // eventually call ShowUI().
  contents_wrapper->ReloadWebContents();

  auto bubble_view = std::make_unique<SelfieCamBubbleDialogView>(
      std::move(contents_wrapper), context_bounds_in_screen);

  bubble_view_ = bubble_view->GetWeakPtr();
  auto* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  // Needed to make the selfie cam round.
  bubble_view_->web_view()->holder()->SetCornerRadii(
      gfx::RoundedCornersF(kCornerRadiusDipLarge));
  // Use Picture-in-Picture (PIP) window management logic for selfie cam so that
  // a) it avoids collision with system UI such as virtual keyboards, quick
  // settings etc.
  // b) it is draggable in tablet mode as well.
  bubble_widget->GetNativeWindow()->SetProperty(ash::kWindowPipTypeKey, true);
  bubble_view_->Redraw();
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
