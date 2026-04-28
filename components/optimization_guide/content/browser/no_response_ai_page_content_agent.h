// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_NO_RESPONSE_AI_PAGE_CONTENT_AGENT_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_NO_RESPONSE_AI_PAGE_CONTENT_AGENT_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
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

  // Forwards the saved request to the real renderer agent.
  void Respond();

  // Closes the fake pipe without replying to the saved request.
  void Disconnect();

 private:
  blink::mojom::AIPageContentOptionsPtr saved_options_;
  GetAIPageContentCallback saved_callback_;
  base::OnceClosure on_request_;
  raw_ptr<content::RenderFrameHost> render_frame_host_;
  mojo::Receiver<blink::mojom::AIPageContentAgent> receiver_{this};
  mojo::Remote<blink::mojom::AIPageContentAgent> agent_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_NO_RESPONSE_AI_PAGE_CONTENT_AGENT_H_
