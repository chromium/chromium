// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_NO_RESPONSE_AI_PAGE_CONTENT_AGENT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_NO_RESPONSE_AI_PAGE_CONTENT_AGENT_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom-test-utils.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace optimization_guide {

// Overrides the AIPageContentAgent interface for the given frame to simulate a
// non-responsive renderer. Tests can either respond later or close the pipe.
class NoResponseAIPageContentAgent
    : public blink::mojom::AIPageContentAgentInterceptorForTesting {
 public:
  explicit NoResponseAIPageContentAgent(
      content::RenderFrameHost* render_frame_host);

  ~NoResponseAIPageContentAgent() override;

  // AIPageContentAgentInterceptorForTesting:
  void GetAIPageContent(blink::mojom::AIPageContentOptionsPtr options,
                        GetAIPageContentCallback callback) override;

  blink::mojom::AIPageContentAgent* GetForwardingInterface() override;

  void Bind(mojo::ScopedMessagePipeHandle handle);

  // Blocks until this fake agent receives an APC request from the browser.
  void WaitForRequest();

  // Forwards the saved requests to the real renderer agent.
  void Respond();

  // Closes the fake pipes without replying to the saved requests.
  void Disconnect();

 private:
  struct SavedRequest {
    SavedRequest(blink::mojom::AIPageContentOptionsPtr options,
                 GetAIPageContentCallback callback);
    SavedRequest(SavedRequest&&) noexcept;
    SavedRequest& operator=(SavedRequest&&) noexcept;
    ~SavedRequest();

    blink::mojom::AIPageContentOptionsPtr options;
    GetAIPageContentCallback callback;
  };

  // Other features may request the page content while a test holds a request
  // back, each on its own pipe. All are held, so a new pipe never drops an
  // earlier request.
  std::vector<SavedRequest> saved_requests_;
  base::OnceClosure on_request_;
  raw_ptr<content::RenderFrameHost> render_frame_host_;
  mojo::ReceiverSet<blink::mojom::AIPageContentAgent> receivers_;
  mojo::Remote<blink::mojom::AIPageContentAgent> agent_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_NO_RESPONSE_AI_PAGE_CONTENT_AGENT_H_
