// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/renderer_url_loader_throttle.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/optional_ref.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"
#include "components/subresource_filter/content/shared/renderer/filter_utils.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace fingerprinting_protection_filter {
namespace {

using ::subresource_filter::mojom::ActivationLevel;
using ::subresource_filter::mojom::ActivationState;

// TODO(https://crbug.com/40280666): Refactor `DocumentSubresourceFilter` to
// use the newer `network::mojom::RequestDestination`, particularly to support
// WebBundles.
blink::mojom::RequestContextType ToRequestContextType(
    network::mojom::RequestDestination request_destination) {
  switch (request_destination) {
    case network::mojom::RequestDestination::kAudio:
      return blink::mojom::RequestContextType::AUDIO;
    case network::mojom::RequestDestination::kEmbed:
      return blink::mojom::RequestContextType::EMBED;
    case network::mojom::RequestDestination::kFont:
      return blink::mojom::RequestContextType::FONT;
    case network::mojom::RequestDestination::kFrame:
    case network::mojom::RequestDestination::kFencedframe:
      return blink::mojom::RequestContextType::FRAME;
    case network::mojom::RequestDestination::kIframe:
      return blink::mojom::RequestContextType::IFRAME;
    case network::mojom::RequestDestination::kImage:
      return blink::mojom::RequestContextType::IMAGE;
    case network::mojom::RequestDestination::kManifest:
      return blink::mojom::RequestContextType::MANIFEST;
    case network::mojom::RequestDestination::kObject:
      return blink::mojom::RequestContextType::OBJECT;
    case network::mojom::RequestDestination::kReport:
      return blink::mojom::RequestContextType::CSP_REPORT;
    case network::mojom::RequestDestination::kScript:
      return blink::mojom::RequestContextType::SCRIPT;
    case network::mojom::RequestDestination::kServiceWorker:
    case network::mojom::RequestDestination::kAudioWorklet:
    case network::mojom::RequestDestination::kPaintWorklet:
    case network::mojom::RequestDestination::kSharedStorageWorklet:
      // Treat worklets like workers in the context of filtering.
      return blink::mojom::RequestContextType::SERVICE_WORKER;
    case network::mojom::RequestDestination::kSharedWorker:
      return blink::mojom::RequestContextType::SHARED_WORKER;
    case network::mojom::RequestDestination::kStyle:
      return blink::mojom::RequestContextType::STYLE;
    case network::mojom::RequestDestination::kTrack:
      return blink::mojom::RequestContextType::TRACK;
    case network::mojom::RequestDestination::kVideo:
      return blink::mojom::RequestContextType::VIDEO;
    case network::mojom::RequestDestination::kWorker:
      return blink::mojom::RequestContextType::WORKER;
    case network::mojom::RequestDestination::kXslt:
      return blink::mojom::RequestContextType::XSLT;
    case network::mojom::RequestDestination::kSpeculationRules:
      return blink::mojom::RequestContextType::SPECULATION_RULES;
    case network::mojom::RequestDestination::kJson:
      return blink::mojom::RequestContextType::JSON;
    case network::mojom::RequestDestination::kEmpty:
    case network::mojom::RequestDestination::kDocument:
    case network::mojom::RequestDestination::kWebBundle:
    case network::mojom::RequestDestination::kWebIdentity:
    case network::mojom::RequestDestination::kDictionary:
    default:
      return blink::mojom::RequestContextType::UNSPECIFIED;
  }
}

}  // namespace

RendererURLLoaderThrottle::RendererURLLoaderThrottle(
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
    base::optional_ref<const blink::LocalFrameToken> local_frame_token)
    : renderer_agent_(nullptr),
      frame_token_(local_frame_token.CopyAsOptional()),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      main_thread_task_runner_(main_thread_task_runner) {
  if (frame_token_.has_value()) {
    // It's only possible to retrieve a `RenderFrame` given a `LocalFrameToken`
    // on the main render thread.
    auto get_renderer_agent_task =
        [](std::optional<blink::LocalFrameToken> frame_token)
        -> base::WeakPtr<RendererAgent> {
      base::WeakPtr<RendererAgent> agent_weakptr = nullptr;
      blink::WebLocalFrame* web_frame =
          blink::WebLocalFrame::FromFrameToken(frame_token.value());
      content::RenderFrame* render_frame = nullptr;
      if (web_frame) {
        render_frame = content::RenderFrame::FromWebFrame(web_frame);
      }
      RendererAgent* renderer_agent = nullptr;
      if (render_frame) {
        renderer_agent = RendererAgent::Get(render_frame);
      }
      if (renderer_agent) {
        agent_weakptr = renderer_agent->GetWeakPtr();
      }

      return agent_weakptr;
    };
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(get_renderer_agent_task, frame_token_)
            .Then(base::BindPostTask(
                task_runner_,
                base::BindOnce(
                    &RendererURLLoaderThrottle::OnRendererAgentLocated,
                    weak_factory_.GetWeakPtr()))));
  } else {
    waiting_for_agent_ = false;
  }
}

RendererURLLoaderThrottle::~RendererURLLoaderThrottle() = default;

// static
bool RendererURLLoaderThrottle::WillIgnoreRequest(
    const GURL& url,
    network::mojom::RequestDestination request_destination) {
  return !url.SchemeIsHTTPOrHTTPS() || net::IsLocalhost(url) ||
         (request_destination !=
              network::mojom::RequestDestination::kWebBundle &&
          request_destination != network::mojom::RequestDestination::kScript);
}

bool RendererURLLoaderThrottle::ShouldAllowRequest(
    subresource_filter::LoadPolicy load_policy) {
  return load_policy == subresource_filter::LoadPolicy::ALLOW ||
         load_policy == subresource_filter::LoadPolicy::WOULD_DISALLOW;
}

void RendererURLLoaderThrottle::CheckCurrentResourceRequest() {
  // This function should only be called after activation is computed.
  CHECK(activation_state_.has_value());

  // Resume immediately if activation is disabled or if we cannot check the
  // filtering ruleset via the agent.
  if ((activation_state_.value().activation_level ==
           ActivationLevel::kDisabled ||
       !main_thread_task_runner_) &&
      deferred_) {
    // Do nothing and resume any deferred requests if activation is disabled.
    deferred_ = false;
    delegate_->Resume();
    return;
  }

  auto check_url_task = [](base::WeakPtr<RendererAgent> agent, GURL url,
                           url_pattern_index::proto::ElementType element_type,
                           RendererAgent::FilterCallback filter_callback) {
    if (agent) {
      agent->CheckURL(url, element_type, std::move(filter_callback));
    } else {
      std::move(filter_callback).Run(subresource_filter::LoadPolicy::ALLOW);
    }
  };
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          check_url_task, renderer_agent_, current_url_,
          subresource_filter::ToElementType(
              ToRequestContextType(request_destination_)),
          base::BindPostTask(
              task_runner_,
              base::BindOnce(&RendererURLLoaderThrottle::OnLoadPolicyCalculated,
                             weak_factory_.GetWeakPtr()))));
}

void RendererURLLoaderThrottle::ProcessRequestStep(const GURL& latest_url,
                                                   bool* defer) {
  current_url_ = latest_url;

  if (WillIgnoreRequest(current_url_, request_destination_)) {
    // Short-circuit on URLs we do not want to filter.
    return;
  }

  // Defer unless we decide it's not needed after checking the request.
  deferred_ = true;
  if (activation_state_.has_value()) {
    // If we know the activation decision, check whether to block the URL.
    CheckCurrentResourceRequest();
  } else if (!waiting_for_agent_) {
    // No way to get activation from the browser - default to disabled.
    ActivationState activation_state;
    activation_state.activation_level = ActivationLevel::kDisabled;
    OnActivationComputed(activation_state);
  }
  if (deferred_) {
    *defer = true;
  }
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

const char* RendererURLLoaderThrottle::NameForLoggingWillProcessResponse() {
  return "FingerprintingProtectionRendererURLLoaderThrottle";
}

void RendererURLLoaderThrottle::OnRendererAgentLocated(
    base::WeakPtr<RendererAgent> renderer_agent) {
  renderer_agent_ = renderer_agent;

  auto get_activation_task =
      [](base::WeakPtr<RendererAgent> agent,
         RendererAgent::ActivationCallback activation_callback) {
        if (agent) {
          agent->GetActivationState(std::move(activation_callback));
        } else {
          std::move(activation_callback).Run(ActivationState());
        }
      };
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          get_activation_task, renderer_agent_,
          base::BindPostTask(
              task_runner_,
              base::BindOnce(&RendererURLLoaderThrottle::OnActivationComputed,
                             weak_factory_.GetWeakPtr()))));
  waiting_for_agent_ = false;
}

void RendererURLLoaderThrottle::OnActivationComputed(
    const ActivationState& activation_state) {
  activation_state_.emplace(activation_state);
  if (deferred_) {
    CheckCurrentResourceRequest();
  }
  waiting_for_agent_ = false;
}

void RendererURLLoaderThrottle::OnLoadPolicyCalculated(
    subresource_filter::LoadPolicy load_policy) {
  if (ShouldAllowRequest(load_policy) ||
      activation_state_.value().activation_level == ActivationLevel::kDryRun) {
    if (deferred_) {
      // Resume if allowed or we are in dry run mode.
      delegate_->Resume();
    }
  } else {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<RendererAgent> agent) {
                         if (agent) {
                           agent->OnSubresourceDisallowed();
                         }
                       },
                       renderer_agent_));
    // Cancel if the resource load should be blocked.
    delegate_->CancelWithError(net::ERR_BLOCKED_BY_CLIENT,
                               "FingerprintingProtection");
  }
  deferred_ = false;
}

}  // namespace fingerprinting_protection_filter
