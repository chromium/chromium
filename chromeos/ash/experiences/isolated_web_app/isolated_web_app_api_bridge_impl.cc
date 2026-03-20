// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/isolated_web_app/isolated_web_app_api_bridge_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notimplemented.h"
#include "chromeos/ash/experiences/isolated_web_app/shaped_window_targeter.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "third_party/blink/public/mojom/chromeos/isolated_web_app_api_bridge.mojom.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// True if the IWA blink extension API is enabled for `render_frame_host`.
bool ApiIsEnabledFor(content::RenderFrameHost& render_frame_host) {
  return render_frame_host.GetWebExposedIsolationLevel() >=
         content::WebExposedIsolationLevel::kIsolatedApplication;
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

}  // namespace

// static
void IsolatedWebAppApiBridgeImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::IsolatedWebAppApiBridge> receiver) {
  CHECK(render_frame_host);
  CHECK(receiver.is_valid());

  if (!ApiIsEnabledFor(*render_frame_host)) {
    return;
  }

  IsolatedWebAppApiBridgeImpl::GetOrCreateForCurrentDocument(render_frame_host)
      ->Bind(std::move(receiver));
}

// static
void IsolatedWebAppApiBridgeImpl::CreateForTesting(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::IsolatedWebAppApiBridge> receiver) {
  CHECK(render_frame_host);
  CHECK(receiver.is_valid());

  IsolatedWebAppApiBridgeImpl* bridge =
      IsolatedWebAppApiBridgeImpl::GetOrCreateForCurrentDocument(
          render_frame_host);
  bridge->force_enable_api_for_testing_ = true;
  bridge->Bind(std::move(receiver));
}

IsolatedWebAppApiBridgeImpl::IsolatedWebAppApiBridgeImpl(
    content::RenderFrameHost* render_frame_host)
    : content::DocumentUserData<IsolatedWebAppApiBridgeImpl>(
          render_frame_host) {}

IsolatedWebAppApiBridgeImpl::~IsolatedWebAppApiBridgeImpl() = default;

void IsolatedWebAppApiBridgeImpl::Bind(
    mojo::PendingReceiver<blink::mojom::IsolatedWebAppApiBridge> receiver) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
}

void IsolatedWebAppApiBridgeImpl::SetShape(const std::vector<gfx::Rect>& rects,
                                           SetShapeCallback callback) {
  if (!force_enable_api_for_testing_ && !ApiIsEnabledFor(render_frame_host())) {
    mojo::ReportBadMessage("SetShape is disabled for this caller.");
    return;
  }

  if (!render_frame_host().IsActive()) {
    // Only active `RenderFrameHost`s should show or update the UI.
    std::move(callback).Run(blink::mojom::SetShapeResult::kNoWindow);
    return;
  }

  views::Widget* widget = GetWidget();
  if (!widget) {
    std::move(callback).Run(blink::mojom::SetShapeResult::kNoWindow);
    return;
  }

  if (rects.size() > blink::mojom::kMaxSetShapeRects) {
    std::move(callback).Run(blink::mojom::SetShapeResult::kInvalidLength);
    return;
  }

  SetShapeAndEventTargeter(*widget, rects);
  std::move(callback).Run(blink::mojom::SetShapeResult::kSuccess);
}

views::Widget* IsolatedWebAppApiBridgeImpl::GetWidget() {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents || !web_contents->GetNativeView()) {
    return nullptr;
  }
  return views::Widget::GetTopLevelWidgetForNativeView(
      web_contents->GetNativeView());
}

DOCUMENT_USER_DATA_KEY_IMPL(IsolatedWebAppApiBridgeImpl);

}  // namespace ash
