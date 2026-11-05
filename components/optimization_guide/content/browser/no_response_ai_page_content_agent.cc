// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/no_response_ai_page_content_agent.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace optimization_guide {

NoResponseAIPageContentAgent::SavedRequest::SavedRequest(
    blink::mojom::AIPageContentOptionsPtr options,
    GetAIPageContentCallback callback)
    : options(std::move(options)), callback(std::move(callback)) {}

NoResponseAIPageContentAgent::SavedRequest::SavedRequest(
    SavedRequest&&) noexcept = default;

NoResponseAIPageContentAgent::SavedRequest&
NoResponseAIPageContentAgent::SavedRequest::operator=(SavedRequest&&) noexcept =
    default;

NoResponseAIPageContentAgent::SavedRequest::~SavedRequest() = default;

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
  saved_requests_.emplace_back(std::move(options), std::move(callback));

  // Let the test disconnect only after the request is actually outstanding.
  if (!on_request_.is_null()) {
    std::move(on_request_).Run();
  }
}

blink::mojom::AIPageContentAgent*
NoResponseAIPageContentAgent::GetForwardingInterface() {
  NOTREACHED();
}

void NoResponseAIPageContentAgent::Bind(mojo::ScopedMessagePipeHandle handle) {
  receivers_.Add(this, mojo::PendingReceiver<blink::mojom::AIPageContentAgent>(
                           std::move(handle)));
}

void NoResponseAIPageContentAgent::WaitForRequest() {
  if (!saved_requests_.empty()) {
    return;
  }

  base::RunLoop run_loop;
  on_request_ = run_loop.QuitClosure();
  run_loop.Run();
}

void NoResponseAIPageContentAgent::Respond() {
  service_manager::InterfaceProvider::TestApi test_api(
      render_frame_host_->GetRemoteInterfaces());

  CHECK(test_api.HasBinderForName(blink::mojom::AIPageContentAgent::Name_));
  test_api.ClearBinderForName(blink::mojom::AIPageContentAgent::Name_);

  render_frame_host_->GetRemoteInterfaces()->GetInterface(
      agent_.BindNewPipeAndPassReceiver());
  for (SavedRequest& request : std::exchange(saved_requests_, {})) {
    agent_->GetAIPageContent(std::move(request.options),
                             std::move(request.callback));
  }
}

void NoResponseAIPageContentAgent::Disconnect() {
  CHECK(!receivers_.empty());

  service_manager::InterfaceProvider::TestApi test_api(
      render_frame_host_->GetRemoteInterfaces());
  CHECK(test_api.HasBinderForName(blink::mojom::AIPageContentAgent::Name_));
  test_api.ClearBinderForName(blink::mojom::AIPageContentAgent::Name_);

  // Closing the receivers without running the saved callbacks exercises the
  // browser-side default callback path for a dropped Mojo response.
  receivers_.Clear();
}

}  // namespace optimization_guide
