// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/sub_apps_renderer_host.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

using blink::mojom::SubAppsProviderResult;

namespace web_app {

SubAppsRendererHost::SubAppsRendererHost(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::SubAppsProvider> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

SubAppsRendererHost::~SubAppsRendererHost() = default;

// static
void SubAppsRendererHost::CreateIfAllowed(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::SubAppsProvider> receiver) {
  // This class is created only on the main frame.
  DCHECK(!render_frame_host->GetParent());

  // Bail if Web Apps aren't enabled on current profile.
  if (!AreWebAppsEnabled(Profile::FromBrowserContext(
          content::WebContents::FromRenderFrameHost(render_frame_host)
              ->GetBrowserContext()))) {
    return;
  }

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new SubAppsRendererHost(render_frame_host, std::move(receiver));
}

void SubAppsRendererHost::Add(AddCallback result_callback) {
  // TODO(isandrk, 1171317): Skeleton implementation for now that just returns
  // an error.
  std::move(result_callback).Run(SubAppsProviderResult::kFailure);
}

}  // namespace web_app
