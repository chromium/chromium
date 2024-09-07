// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/renderer_url_loader_throttle.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_ref.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_errors.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace fingerprinting_protection_filter {
namespace {

using ::subresource_filter::mojom::ActivationLevel;

}  // namespace

RendererURLLoaderThrottle::RendererURLLoaderThrottle(
    RendererAgent* renderer_agent,
    base::optional_ref<const blink::LocalFrameToken> local_frame_token)
    : renderer_agent_(renderer_agent),
      frame_token_(local_frame_token.CopyAsOptional()),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  if (renderer_agent_) {
    renderer_agent_->AddThrottle(this);
  }
}

RendererURLLoaderThrottle::~RendererURLLoaderThrottle() {
  if (renderer_agent_) {
    renderer_agent_->DeleteThrottle(this);
  }
}

// static
bool RendererURLLoaderThrottle::WillIgnoreRequest(
    const GURL& url,
    network::mojom::RequestDestination request_destination) {
  return !url.SchemeIsHTTPOrHTTPS() ||
         (request_destination !=
              network::mojom::RequestDestination::kWebBundle &&
          request_destination != network::mojom::RequestDestination::kScript);
}

bool RendererURLLoaderThrottle::ShouldAllowRequest() {
  // TODO(https://crbug.com/40280666): Implement once a filter is integrated
  // with the `DocumentLoader`.
  return true;
}

void RendererURLLoaderThrottle::CheckCurrentResourceRequest() {
  // This function should only be called after activation is computed.
  CHECK(activation_state_.has_value());

  if (activation_state_.value().activation_level ==
          ActivationLevel::kDisabled &&
      deferred_) {
    // Do nothing and resume any deferred requests if activation is disabled.
    deferred_ = false;
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](blink::URLLoaderThrottle::Delegate* delegate) {
                         if (delegate) {
                           delegate->Resume();
                         }
                       },
                       delegate_));
    return;
  }

  if (ShouldAllowRequest() ||
      activation_state_.value().activation_level == ActivationLevel::kDryRun) {
    if (deferred_) {
      // Resume if allowed or we are in dry run mode.
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(
                         [](blink::URLLoaderThrottle::Delegate* delegate) {
                           if (delegate) {
                             delegate->Resume();
                           }
                         },
                         delegate_));
    }
  } else {
    // Cancel if the resource load should be blocked.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](blink::URLLoaderThrottle::Delegate* delegate) {
                         if (delegate) {
                           delegate->CancelWithError(
                               net::ERR_BLOCKED_BY_CLIENT,
                               "FingerprintingProtection");
                         }
                       },
                       delegate_));
  }
  deferred_ = false;
}

void RendererURLLoaderThrottle::ProcessRequestStep(const GURL& latest_url,
                                                   bool* defer) {
  current_url_ = latest_url;

  if (WillIgnoreRequest(current_url_, request_destination_)) {
    // Short-circuit on URLs we do not want to filter.
    return;
  }

  deferred_ = true;
  if (activation_state_.has_value()) {
    // If we know the activation decision, check whether to block the URL.
    CheckCurrentResourceRequest();
  } else if (renderer_agent_ && !renderer_agent_->IsPendingActivation()) {
    OnActivationComputed(renderer_agent_->GetActivationState());
  } else if (!renderer_agent_) {
    // No way to get activation from the browser - default to disabled.
    subresource_filter::mojom::ActivationState activation_state;
    activation_state.activation_level =
        subresource_filter::mojom::ActivationLevel::kDisabled;
    OnActivationComputed(activation_state);
  }
  if (deferred_) {
    *defer = true;
  }
}

void RendererURLLoaderThrottle::DetachFromCurrentSequence() {
  // Tasks should always be run on the current sequence.
  task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
}

void RendererURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  request_destination_ = request->destination;
  ProcessRequestStep(request->url, defer);
}

void RendererURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers,
    net::HttpRequestHeaders* modified_cors_exempt_headers) {
  ProcessRequestStep(redirect_info->new_url, defer);
}

void RendererURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  ProcessRequestStep(response_url, defer);
}

const char* RendererURLLoaderThrottle::NameForLoggingWillProcessResponse() {
  return "FingerprintingProtectionRendererURLLoaderThrottle";
}

void RendererURLLoaderThrottle::OnActivationComputed(
    const subresource_filter::mojom::ActivationState& activation_state) {
  activation_state_.emplace(activation_state);
  if (deferred_) {
    CheckCurrentResourceRequest();
  }
}

}  // namespace fingerprinting_protection_filter
