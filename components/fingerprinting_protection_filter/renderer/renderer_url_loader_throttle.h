// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_ref.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {

class RendererAgent;

// `RendererURLLoaderThrottle` is used in renderer processes to check if URLs
// match the Fingerprinting Protection ruleset. It defers response processing
// until all URL checks are completed and cancels the load if it receives a
// signal to activate from the browser process and a URL matches the ruleset.
//
// One throttle will be instantiated per resource load (i.e. possibly multiple
// per `RenderFrame`).
class RendererURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  RendererURLLoaderThrottle(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      base::optional_ref<const blink::LocalFrameToken> local_frame_token);

  RendererURLLoaderThrottle(const RendererURLLoaderThrottle&) = delete;
  RendererURLLoaderThrottle& operator=(const RendererURLLoaderThrottle&) =
      delete;

  ~RendererURLLoaderThrottle() override;

  // Returns whether we will check `url` against the filtering ruleset based on
  // scheme, request destination (i.e. file type), etc.
  static bool WillIgnoreRequest(
      const GURL& url,
      network::mojom::RequestDestination request_destination);

  // Callback to notify throttles of their associated `RendererAgent`.
  // Should be passed to a task runner on the main thread.
  void OnRendererAgentLocated(base::WeakPtr<RendererAgent> renderer_agent);

  // Callback to notify throttles of page-level activation (i.e. whether to
  // attempt filtering or simply resume the request). Should be passed to
  // a `RendererAgent` associated with the same `RenderFrame` as the throttle.
  void OnActivationComputed(
      const subresource_filter::mojom::ActivationState& activation_state);

  // Callback to notify throttles of the policy to apply to a URL that has been
  // checked against the filtering ruleset. Should be passed to a
  // `RendererAgent` associated with the same `RenderFrame` as the throttle.
  void OnLoadPolicyCalculated(subresource_filter::LoadPolicy load_policy);

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;

  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override;

  const char* NameForLoggingWillProcessResponse() override;

  base::WeakPtr<RendererURLLoaderThrottle> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  // This function is protected virtual to allow mocking in tests.
  virtual bool ShouldAllowRequest(subresource_filter::LoadPolicy load_policy);

  GURL GetCurrentURL() { return current_url_; }

  // Only to be used to inject an agent in unittests in the absence of a
  // frame.
  void SetRendererAgentForTesting(base::WeakPtr<RendererAgent> renderer_agent) {
    // Since we don't have a `RenderFrame` and can't set the agent the normal
    // way, we need to pretend we are waiting for it for resource loads to be
    // deferred until activation arrives.
    waiting_for_agent_ = true;
    renderer_agent_ = renderer_agent;
  }

 private:
  // Checks whether filtering is activated or not, and if so, whether the URL
  // for the current resource request matches a filtering rule. Cancels the
  // request if there is a match, or resumes it otherwise.
  void CheckCurrentResourceRequest();

  // Utility function used in the blink::URLLoaderThrottle implementation to
  // defer a resource request if we are still waiting for activation to be
  // computed or check whether it should be filtered otherwise.
  void ProcessRequestStep(const GURL& latest_url, bool* defer);

  // Whether we are still waiting for the `RendererAgent` that this throttle's
  // request corresponds to to be retrieved.
  bool waiting_for_agent_ = true;

  // Lives on a different thread, as `RendererAgent` instances are owned by
  // `ChromeContentRendererClient`. Must be dereferenced on the main thread.
  base::WeakPtr<RendererAgent> renderer_agent_;

  const std::optional<blink::LocalFrameToken> frame_token_;

  GURL current_url_;
  network::mojom::RequestDestination request_destination_;
  bool deferred_ = false;
  std::optional<subresource_filter::mojom::ActivationState> activation_state_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;

  base::WeakPtrFactory<RendererURLLoaderThrottle> weak_factory_{this};
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_
