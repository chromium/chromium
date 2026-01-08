// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/no_response_ai_page_content_agent.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace optimization_guide {

NoResponseAIPageContentAgent::NoResponseAIPageContentAgent(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
  service_manager::InterfaceProvider::TestApi(
      render_frame_host_->GetRemoteInterfaces())
      .SetBinderForName(blink::mojom::AIPageContentAgent::Name_,
                        base::BindRepeating(&NoResponseAIPageContentAgent::Bind,
                                            base::Unretained(this)));
}

NoResponseAIPageContentAgent::~NoResponseAIPageContentAgent() = default;

void NoResponseAIPageContentAgent::GetAIPageContent(
    blink::mojom::AIPageContentOptionsPtr options,
    GetAIPageContentCallback callback) {
  // Do nothing, simulating a non-responsive renderer, but save the arguments
  // to allow later processing.
  saved_options_ = std::move(options);
  saved_callback_ = std::move(callback);
}

blink::mojom::AIPageContentAgent*
NoResponseAIPageContentAgent::GetForwardingInterface() {
  NOTREACHED();
}

void NoResponseAIPageContentAgent::Bind(mojo::ScopedMessagePipeHandle handle) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(mojo::PendingReceiver<blink::mojom::AIPageContentAgent>(
      std::move(handle)));
}

void NoResponseAIPageContentAgent::Respond() {
  service_manager::InterfaceProvider::TestApi test_api(
      render_frame_host_->GetRemoteInterfaces());

  CHECK(test_api.HasBinderForName(blink::mojom::AIPageContentAgent::Name_));
  test_api.ClearBinderForName(blink::mojom::AIPageContentAgent::Name_);

  render_frame_host_->GetRemoteInterfaces()->GetInterface(
      agent_.BindNewPipeAndPassReceiver());
  agent_->GetAIPageContent(std::move(saved_options_),
                           std::move(saved_callback_));
}

}  // namespace optimization_guide
