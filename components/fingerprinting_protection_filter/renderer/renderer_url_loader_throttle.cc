// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/renderer_url_loader_throttle.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"
#include "components/subresource_filter/content/shared/renderer/filter_utils.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "components/variations/variations_switches.h"
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

using ::subresource_filter::LoadPolicy;
using ::subresource_filter::mojom::ActivationLevel;
using ::subresource_filter::mojom::ActivationState;

void RecordDeferTimeHistogram(ActivationLevel activation_level,
                              LoadPolicy load_policy,
                              base::TimeTicks defer_time) {
  auto total_defer_time = base::TimeTicks::Now() - defer_time;
  if (activation_level == ActivationLevel::kDisabled) {
    UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
        "FingerprintingProtection.SubresourceLoad.TotalDeferTime."
        "ActivationDisabled",
        total_defer_time, base::Microseconds(1), base::Seconds(10), 50);
  } else if (load_policy == LoadPolicy::ALLOW) {
    UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
        "FingerprintingProtection.SubresourceLoad.TotalDeferTime.Allowed",
        total_defer_time, base::Microseconds(1), base::Seconds(10), 50);
  } else if (load_policy == LoadPolicy::WOULD_DISALLOW) {
    UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
        "FingerprintingProtection.SubresourceLoad.TotalDeferTime.WouldDisallow",
        total_defer_time, base::Microseconds(1), base::Seconds(10), 50);
  } else {
    UMA_HISTOGRAM_CUSTOM_MICRO_TIMES(
        "FingerprintingProtection.SubresourceLoad.TotalDeferTime.Disallowed",
        total_defer_time, base::Microseconds(1), base::Seconds(10), 50);
  }
}

}  // namespace

RendererURLLoaderThrottle::RendererURLLoaderThrottle(
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
    const blink::LocalFrameToken& local_frame_token)
    : renderer_agent_(nullptr),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      main_thread_task_runner_(main_thread_task_runner) {
  if (main_thread_task_runner_) {
    // It's only possible to retrieve a `RenderFrame` given a `LocalFrameToken`
    // on the main render thread.
    auto get_renderer_agent_task = [](const blink::LocalFrameToken& frame_token)
        -> base::WeakPtr<RendererAgent> {
      base::WeakPtr<RendererAgent> agent_weakptr = nullptr;
      blink::WebLocalFrame* web_frame =
          blink::WebLocalFrame::FromFrameToken(frame_token);
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
        base::BindOnce(get_renderer_agent_task, local_frame_token)
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
  bool should_exclude_localhost =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          variations::switches::kEnableBenchmarking) &&
      net::IsLocalhost(url);
  return !url.SchemeIsHTTPOrHTTPS() || should_exclude_localhost ||
         (request_destination !=
              network::mojom::RequestDestination::kWebBundle &&
          request_destination != network::mojom::RequestDestination::kScript);
}

bool RendererURLLoaderThrottle::ShouldAllowRequest() {
  if (!load_policy_.has_value()) {
    return true;
  }
  LoadPolicy load_policy = load_policy_.value();
  return load_policy == LoadPolicy::EXPLICITLY_ALLOW ||
         load_policy == LoadPolicy::ALLOW ||
         load_policy == LoadPolicy::WOULD_DISALLOW;
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
    RecordDeferTimeHistogram(ActivationLevel::kDisabled, LoadPolicy::ALLOW,
                             defer_timestamp_);
    return;
  }
  auto check_url_task = [](base::WeakPtr<RendererAgent> agent, GURL url,
                           std::optional<std::string> devtools_request_id,
                           url_pattern_index::proto::ElementType element_type,
                           RendererAgent::FilterCallback filter_callback) {
    if (agent) {
      agent->CheckURL(url, devtools_request_id, element_type,
                      std::move(filter_callback));
    } else {
      std::move(filter_callback).Run(LoadPolicy::ALLOW);
    }
  };
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          check_url_task, renderer_agent_, current_url_, devtools_request_id_,
          subresource_filter::ToElementType(request_destination_),
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
  // Defer unless activation is disabled or we decide it's not needed after
  // checking the request.
  deferred_ = activation_state_.has_value()
                  ? (activation_state_.value().activation_level !=
                     ActivationLevel::kDisabled)
                  : true;
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
    defer_timestamp_ = base::TimeTicks::Now();
    *defer = true;
  }
}

void RendererURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  request_destination_ = request->destination;
  devtools_request_id_ = request->devtools_request_id;
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

void RendererURLLoaderThrottle::OnLoadPolicyCalculated(LoadPolicy load_policy) {
  load_policy_ = load_policy;
  if (ShouldAllowRequest() ||
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
    delegate_->CancelWithError(net::ERR_BLOCKED_BY_FINGERPRINTING_PROTECTION,
                               "FingerprintingProtection");
  }
  if (deferred_) {
    RecordDeferTimeHistogram(activation_state_.value().activation_level,
                             load_policy_.value(), defer_timestamp_);
  }
  deferred_ = false;
}

}  // namespace fingerprinting_protection_filter
