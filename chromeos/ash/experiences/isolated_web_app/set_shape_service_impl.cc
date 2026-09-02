// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/isolated_web_app/set_shape_service_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notimplemented.h"
#include "chromeos/ash/experiences/isolated_web_app/isolated_web_app_api_allowlist.h"
#include "chromeos/ash/experiences/isolated_web_app/shaped_window_targeter.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/set_shape/set_shape.mojom.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "url/origin.h"

namespace ash {

namespace {

// True if the IWA blink extension API is enabled for `render_frame_host`.
bool ApiIsEnabledFor(content::RenderFrameHost& render_frame_host) {
  if (render_frame_host.GetWebExposedIsolationLevel() <
      content::WebExposedIsolationLevel::kIsolatedApplication) {
    return false;
  }

  if (CanOriginAccessCrosIwaApi(render_frame_host.GetLastCommittedOrigin())) {
    return true;
  }

  return base::FeatureList::IsEnabled(blink::features::kSetShape);
}

void SetShapeAndEventTargeter(views::Widget& widget,
                              const std::vector<gfx::Rect>& rects) {
  if (rects.empty()) {
    widget.SetShape(nullptr);
    widget.GetNativeWindow()->SetEventTargeter(nullptr);
  } else {
    widget.SetShape(std::make_unique<views::Widget::ShapeRects>(rects));
    widget.GetNativeWindow()->SetEventTargeter(
        std::make_unique<ShapedWindowTargeter>(rects));
  }
}

// Returns true if `rect` has dimensions of at least `kMinimumIwaSetShapeSize`.
bool IsAtLeastMinimumSize(const gfx::Rect& rect) {
  return rect.width() >= blink::mojom::kMinimumIwaSetShapeSize &&
         rect.height() >= blink::mojom::kMinimumIwaSetShapeSize;
}

// Returns the window bounds with 1px tolerance on width and height.
//
// The 1px tolerance accounts for rounding differences between window sizes in
// Blink and Views in screens with fractional device scale factors.
gfx::Rect WindowBoundsWithTolerance(const gfx::Size& window_size) {
  return gfx::Rect(0, 0, window_size.width() + 1, window_size.height() + 1);
}

// Returns true if `rect` has dimensions of at least `kMinimumIwaSetShapeSize`
// within `window_bounds`.
bool IsAtLeastMinimumSizeInBounds(const gfx::Rect& rect,
                                  const gfx::Rect& window_bounds) {
  gfx::Rect visible_rect = gfx::IntersectRects(rect, window_bounds);
  return visible_rect.width() >= blink::mojom::kMinimumIwaSetShapeSize &&
         visible_rect.height() >= blink::mojom::kMinimumIwaSetShapeSize;
}

}  // namespace

// static
void SetShapeServiceImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::SetShapeService> receiver) {
  CHECK(render_frame_host);
  CHECK(receiver.is_valid());

  if (!ApiIsEnabledFor(*render_frame_host)) {
    return;
  }

  SetShapeServiceImpl::GetOrCreateForCurrentDocument(render_frame_host)
      ->Bind(std::move(receiver));
}

// static
void SetShapeServiceImpl::CreateForTesting(  // IN-TEST
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::SetShapeService> receiver) {
  CHECK(render_frame_host);
  CHECK(receiver.is_valid());

  SetShapeServiceImpl* bridge =
      SetShapeServiceImpl::GetOrCreateForCurrentDocument(render_frame_host);
  bridge->force_enable_api_for_testing_ = true;
  bridge->Bind(std::move(receiver));
}

SetShapeServiceImpl::SetShapeServiceImpl(
    content::RenderFrameHost* render_frame_host)
    : content::DocumentUserData<SetShapeServiceImpl>(render_frame_host) {}

SetShapeServiceImpl::~SetShapeServiceImpl() {
  UnsubscribeFromWindowManagementPermissionChanges();
}

void SetShapeServiceImpl::Bind(
    mojo::PendingReceiver<blink::mojom::SetShapeService> receiver) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
}

void SetShapeServiceImpl::SetShape(const std::vector<gfx::Rect>& rects,
                                   SetShapeCallback callback) {
  if (!force_enable_api_for_testing_ && !ApiIsEnabledFor(render_frame_host())) {
    receiver_.ReportBadMessage("SetShape is disabled for this caller.");
    return;
  }

  if (!render_frame_host().IsActive()) {
    // Only active `RenderFrameHost`s should show or update the UI.
    std::move(callback).Run(blink::mojom::SetShapeResult::kNoWindow);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    std::move(callback).Run(blink::mojom::SetShapeResult::kNoWindow);
    return;
  }

  blink::mojom::DisplayMode display_mode = blink::mojom::DisplayMode::kBrowser;
  if (web_contents->GetDelegate()) {
    display_mode = web_contents->GetDelegate()->GetDisplayMode(web_contents);
  }

  if (display_mode != blink::mojom::DisplayMode::kUnframed) {
    std::move(callback).Run(blink::mojom::SetShapeResult::kNotUnframed);
    return;
  }

  views::Widget* widget = GetWidget();
  if (!widget) {
    std::move(callback).Run(blink::mojom::SetShapeResult::kNoWindow);
    return;
  }

  if (rects.size() > blink::mojom::kMaxSetShapeRects) {
    receiver_.ReportBadMessage("SetShape called with too many rects.");
    return;
  }

  if (!rects.empty() && std::ranges::none_of(rects, &IsAtLeastMinimumSize)) {
    receiver_.ReportBadMessage(
        "SetShape called with invalid shape (no rect meets minimum size "
        "requirement).");
    return;
  }

  const gfx::Rect bounds =
      WindowBoundsWithTolerance(widget->GetWindowBoundsInScreen().size());
  if (!rects.empty() && std::ranges::none_of(rects, [&](const gfx::Rect& rect) {
        return IsAtLeastMinimumSizeInBounds(rect, bounds);
      })) {
    // This is not a `ReportBadMessage` because it can happen if there is a race
    // between the `setShape` call and widget resizing.
    //
    // A well-behaving renderer could check the shape, find it is valid for the
    // window size, and make the mojo call. Meanwhile, the window resize happens
    // asynchronously. Then this service receives the mojo call, checks the
    // shape against the latest window bounds, and finds it is not valid.
    std::move(callback).Run(blink::mojom::SetShapeResult::kOutOfBounds);
    return;
  }

  SetShapeAndEventTargeter(*widget, rects);

  if (rects.empty()) {
    UnsubscribeFromWindowManagementPermissionChanges();
  } else {
    SubscribeToWindowManagementPermissionChanges();
  }

  std::move(callback).Run(blink::mojom::SetShapeResult::kSuccess);
}

void SetShapeServiceImpl::ResetShape() {
  views::Widget* widget = GetWidget();
  if (widget) {
    SetShapeAndEventTargeter(*widget, {});
  }
}

void SetShapeServiceImpl::OnWindowManagementPermissionChanged(
    content::PermissionResult result) {
  if (result.status != blink::mojom::PermissionStatus::GRANTED) {
    ResetShape();
    UnsubscribeFromWindowManagementPermissionChanges();
  }
}

void SetShapeServiceImpl::SubscribeToWindowManagementPermissionChanges() {
  if (permission_subscription_id_) {
    return;
  }

  auto* controller =
      render_frame_host().GetBrowserContext()->GetPermissionController();
  url::Origin origin = render_frame_host().GetLastCommittedOrigin();

  permission_subscription_id_ = controller->SubscribeToPermissionResultChange(
      content::PermissionDescriptorUtil::
          CreatePermissionDescriptorForPermissionType(
              blink::PermissionType::WINDOW_MANAGEMENT),
      /*render_process_host*/ nullptr, &render_frame_host(), origin.GetURL(),
      /*should_include_device_status=*/false,
      base::BindRepeating(
          &SetShapeServiceImpl::OnWindowManagementPermissionChanged,
          base::Unretained(this)));
}

void SetShapeServiceImpl::UnsubscribeFromWindowManagementPermissionChanges() {
  if (!permission_subscription_id_) {
    return;
  }

  render_frame_host()
      .GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(*permission_subscription_id_);
  permission_subscription_id_.reset();
}

views::Widget* SetShapeServiceImpl::GetWidget() {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents || !web_contents->GetNativeView()) {
    return nullptr;
  }
  return views::Widget::GetTopLevelWidgetForNativeView(
      web_contents->GetNativeView());
}

DOCUMENT_USER_DATA_KEY_IMPL(SetShapeServiceImpl);

}  // namespace ash
