// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/isolated_web_app/isolated_web_app_api_bridge_impl.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notimplemented.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

// True if the IWA blink extension API is enabled for `render_frame_host`.
bool ApiIsEnabledFor(content::RenderFrameHost& render_frame_host) {
  return render_frame_host.GetWebExposedIsolationLevel() >=
         content::WebExposedIsolationLevel::kIsolatedApplication;
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
  if (!ApiIsEnabledFor(render_frame_host())) {
    mojo::ReportBadMessage("SetShape is disabled for this caller.");
    return;
  }

  if (rects.size() > blink::mojom::kMaxSetShapeRects) {
    std::move(callback).Run(blink::mojom::SetShapeResult::kInvalidLength);
    return;
  }

  // TODO(crbug.com/480146201): Implement `SetShape`.
  NOTIMPLEMENTED();
  std::move(callback).Run(blink::mojom::SetShapeResult::kSuccess);
}

DOCUMENT_USER_DATA_KEY_IMPL(IsolatedWebAppApiBridgeImpl);

}  // namespace ash
