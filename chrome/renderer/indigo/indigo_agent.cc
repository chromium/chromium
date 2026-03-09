// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/indigo/indigo_agent.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"

namespace indigo {

// static
void IndigoAgent::MaybeCreate(content::RenderFrame* render_frame,
                              blink::AssociatedInterfaceRegistry* registry) {
  if (render_frame->IsMainFrame() &&
      base::FeatureList::IsEnabled(features::kIndigo)) {
    new IndigoAgent(render_frame, registry);
  }
}

IndigoAgent::IndigoAgent(content::RenderFrame* render_frame,
                         blink::AssociatedInterfaceRegistry* registry)
    : content::RenderFrameObserver(render_frame) {
  registry->AddInterface<chrome::mojom::IndigoAgent>(
      base::BindRepeating(&IndigoAgent::BindReceiver, base::Unretained(this)));
}

IndigoAgent::~IndigoAgent() = default;

void IndigoAgent::InjectScript(const std::string& script_content,
                               const GURL& script_url,
                               const url::Origin& origin,
                               base::OnceClosure done) {
  // Associate the isolated world with the provided origin.
  blink::WebIsolatedWorldInfo info;
  info.human_readable_name = "Indigo";
  info.security_origin = blink::WebSecurityOrigin(origin);
  blink::SetIsolatedWorldInfo(ISOLATED_WORLD_ID_INDIGO, info);

  blink::WebScriptSource source(blink::WebString::FromUTF8(script_content),
                                script_url);
  render_frame()->GetWebFrame()->ExecuteScriptInIsolatedWorld(
      ISOLATED_WORLD_ID_INDIGO, source, blink::BackForwardCacheAware::kAllow);
  std::move(done).Run();
}

void IndigoAgent::OnDestruct() {
  delete this;
}

void IndigoAgent::BindReceiver(
    mojo::PendingAssociatedReceiver<chrome::mojom::IndigoAgent>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

}  // namespace indigo
