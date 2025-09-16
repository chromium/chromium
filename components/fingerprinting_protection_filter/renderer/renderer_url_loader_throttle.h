// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/fingerprinting_protection_filter/common/throttle_creation_result.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace subresource_filter {
class DocumentSubresourceFilter;
class MemoryMappedRuleset;
}  // namespace subresource_filter

namespace fingerprinting_protection_filter {

// `RendererURLLoaderThrottle` is used in renderer processes to check if URLs
// match the Fingerprinting Protection ruleset. It defers response processing
// until all URL checks are completed and cancels the load if it receives a
// signal to activate from the browser process and a URL matches the ruleset.
//
// One throttle will be instantiated per resource load (i.e. possibly multiple
// per `RenderFrame`).
class RendererURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  // Should only be used by unit tests to inject a RendererAgent* in the
  // absence of a RendererFrame to retrieve it from.
  static std::unique_ptr<RendererURLLoaderThrottle> CreateForTesting(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      scoped_refptr<const subresource_filter::MemoryMappedRuleset>
          filtering_ruleset,
      base::OnceCallback<RendererAgent*()> renderer_agent_getter);

  RendererURLLoaderThrottle(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      const blink::LocalFrameToken& local_frame_token,
      scoped_refptr<const subresource_filter::MemoryMappedRuleset>
          filtering_ruleset);

  RendererURLLoaderThrottle(const RendererURLLoaderThrottle&) = delete;
  RendererURLLoaderThrottle& operator=(const RendererURLLoaderThrottle&) =
      delete;

  ~RendererURLLoaderThrottle() override;

  // Returns `std::nullopt` if we will check `url` against the filtering
  // ruleset based on scheme, request destination (i.e. file type), etc.
  // Otherwise, returns a `ThrottleCreationResult` describing why the request
  // will not be checked.
  static std::optional<RendererThrottleCreationResult> WillIgnoreRequest(
      const GURL& url,
      network::mojom::RequestDestination request_destination);

  // Callback to notify throttles of the activation state to apply when
  // deciding whether to apply filtering to their subresource URL. Should be
  // passed to a `RendererAgent` associated with the same `RenderFrame` as the
  // throttle.
  void OnActivationComputed(
      subresource_filter::mojom::ActivationState activation_state,
      RendererAgent::OnSubresourceEvaluatedCallback
          on_subresource_evaluated_callback,
      const GURL& current_document_url);

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

  std::optional<subresource_filter::mojom::ActivationLevel>
  GetCurrentActivation() {
    if (activation_state_.has_value()) {
      return activation_state_->activation_level;
    }
    return std::nullopt;
  }

 protected:
  // This function is protected virtual to allow mocking in tests.
  virtual bool ShouldAllowRequest();

  GURL current_url() { return current_url_; }

  // The `LoadPolicy` returned by the ruleset check, if any.
  std::optional<subresource_filter::LoadPolicy> load_policy_;

 private:
  // Constructor that allows injecting a RendererAgent in unit tests where
  // there is no RenderFrame to retrieve it from.
  RendererURLLoaderThrottle(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      scoped_refptr<const subresource_filter::MemoryMappedRuleset>
          filtering_ruleset,
      base::OnceCallback<RendererAgent*()> renderer_agent_getter);

  // Utility function used in the blink::URLLoaderThrottle implementation to
  // defer a resource request if we are still waiting for activation to be
  // computed or check whether it should be filtered otherwise.
  void ProcessRequestStep(const GURL& latest_url, bool* defer);

  // Whether we are still waiting for the `RendererAgent` that this throttle's
  // request corresponds to get the activation state for the URL we are
  // checking.
  bool activation_computed_ = false;

  // Callback used to notify the RendererAgent that a subresource has been
  // evaluated. The callback runs on the main thread.
  RendererAgent::OnSubresourceEvaluatedCallback
      on_subresource_evaluated_callback_;

  // The URL of the document within which the current subresource load request
  // originated. Set via callback by the `RendererAgent`.
  GURL current_document_url_;

  // The URL for the subresource that this throttle may or may not defer.
  GURL current_url_;
  std::optional<network::mojom::RequestDestination> request_destination_;
  std::optional<std::string> devtools_request_id_;
  bool deferred_ = false;
  std::optional<subresource_filter::mojom::ActivationState> activation_state_;

  // Time tracking for metrics collection.
  base::TimeTicks defer_timestamp_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;

  // A pointer to the ruleset to use for filtering if activation is enabled.
  // Throttles should not be created if a ruleset is not available, so this
  // should always be a valid pointer.
  scoped_refptr<const subresource_filter::MemoryMappedRuleset>
      filtering_ruleset_;
  // Will be conditionally initialized once the activation state is retrieved
  // from the `RendererAgent`.
  std::unique_ptr<subresource_filter::DocumentSubresourceFilter> filter_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RendererURLLoaderThrottle> weak_factory_{this};
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_RENDERER_RENDERER_URL_LOADER_THROTTLE_H_
