// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/renderer_url_loader_throttle.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"
#include "components/subresource_filter/content/shared/renderer/filter_utils.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
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

// Should be called on the main thread.
RendererAgent* GetRendererAgent(
    const blink::LocalFrameToken& local_frame_token) {
  blink::WebLocalFrame* web_frame =
      blink::WebLocalFrame::FromFrameToken(local_frame_token);

  if (!web_frame) {
    return nullptr;
  }

  auto* render_frame = content::RenderFrame::FromWebFrame(web_frame);
  if (!render_frame) {
    return nullptr;
  }
  return RendererAgent::Get(render_frame);
}

}  // namespace

RendererURLLoaderThrottle::RendererURLLoaderThrottle(
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
    const blink::LocalFrameToken& local_frame_token,
    scoped_refptr<const subresource_filter::MemoryMappedRuleset>
        filtering_ruleset)
    : RendererURLLoaderThrottle(
          main_thread_task_runner,
          filtering_ruleset,
          base::BindOnce(&GetRendererAgent, local_frame_token)) {}

RendererURLLoaderThrottle::RendererURLLoaderThrottle(
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
    scoped_refptr<const subresource_filter::MemoryMappedRuleset>
        filtering_ruleset,
    base::OnceCallback<RendererAgent*()> renderer_agent_getter)
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      main_thread_task_runner_(main_thread_task_runner),
      filtering_ruleset_(filtering_ruleset) {
  // It's only possible to retrieve a `RenderFrame` given a `LocalFrameToken`
  // on the main render thread.
  CHECK(main_thread_task_runner_);

  auto set_activation_computed_callback =
      [](base::OnceCallback<RendererAgent*()> renderer_agent_getter,
         RendererAgent::ActivationComputedCallback
             activation_computed_callback) {
        auto* renderer_agent = std::move(renderer_agent_getter).Run();
        if (!renderer_agent) {
          return;
        }

        renderer_agent->AddActivationComputedCallback(
            std::move(activation_computed_callback));
      };

  auto activated_computed_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&RendererURLLoaderThrottle::OnActivationComputed,
                     weak_factory_.GetWeakPtr()));
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(set_activation_computed_callback,
                                std::move(renderer_agent_getter),
                                std::move(activated_computed_callback)));
}

RendererURLLoaderThrottle::~RendererURLLoaderThrottle() = default;

// static
std::unique_ptr<RendererURLLoaderThrottle>
RendererURLLoaderThrottle::CreateForTesting(
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
    scoped_refptr<const subresource_filter::MemoryMappedRuleset>
        filtering_ruleset,
    base::OnceCallback<RendererAgent*()> renderer_agent_getter) {
  return base::WrapUnique(
      new RendererURLLoaderThrottle(main_thread_task_runner, filtering_ruleset,
                                    std::move(renderer_agent_getter)));
}

// static
std::optional<RendererThrottleCreationResult>
RendererURLLoaderThrottle::WillIgnoreRequest(
    const GURL& url,
    network::mojom::RequestDestination request_destination) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return RendererThrottleCreationResult::kSkipNonHttp;
  }
  bool should_exclude_localhost =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          variations::switches::kEnableBenchmarking) &&
      net::IsLocalhost(url);
  if (should_exclude_localhost) {
    return RendererThrottleCreationResult::kSkipLocalHost;
  }
  if (request_destination != network::mojom::RequestDestination::kWebBundle &&
      request_destination != network::mojom::RequestDestination::kScript) {
    return RendererThrottleCreationResult::kSkipSubresourceType;
  }
  return std::nullopt;
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

void RendererURLLoaderThrottle::ProcessRequestStep(const GURL& latest_url,
                                                   bool* defer) {
  current_url_ = latest_url;

  if (WillIgnoreRequest(current_url_,
                        request_destination_.value_or(
                            network::mojom::RequestDestination::kEmpty))
          .has_value()) {
    // Short-circuit on URLs we do not want to filter or if there is no
    // filtering ruleset to use.
    return;
  }

  if (!activation_computed_) {
    deferred_ = true;
    defer_timestamp_ = base::TimeTicks::Now();
    *defer = true;
  } else {
    OnActivationComputed(activation_state_.value(),
                         on_subresource_evaluated_callback_,
                         current_document_url_);
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

void RendererURLLoaderThrottle::OnActivationComputed(
    subresource_filter::mojom::ActivationState activation_state,
    RendererAgent::OnSubresourceEvaluatedCallback on_subresource_callback,
    const GURL& current_document_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!request_destination_.has_value()) {
    // This means `OnActivationComputed` was called before `WillStartRequest`.
    // We want to know if this scenario actually occurs in production.
    base::debug::DumpWithoutCrashing();
  }

  activation_state_ = activation_state;
  on_subresource_evaluated_callback_ = on_subresource_callback;
  current_document_url_ = current_document_url;
  activation_computed_ = true;

  if (activation_state_->activation_level != ActivationLevel::kDisabled) {
    if (!filter_) {
      url::Origin origin = url::Origin::Create(current_document_url);
      filter_ = std::make_unique<subresource_filter::DocumentSubresourceFilter>(
          std::move(origin), activation_state_.value(),
          std::move(filtering_ruleset_),
          kFingerprintingProtectionRulesetConfig.uma_tag);
    }
  }

  if (filter_ && current_url_ != GURL() && request_destination_.has_value()) {
    load_policy_ = filter_->GetLoadPolicy(
        current_url_,
        subresource_filter::ToElementType(request_destination_.value()));
  } else {
    load_policy_ = LoadPolicy::ALLOW;
  }

  bool subresource_disallowed = false;
  if (ShouldAllowRequest() ||
      activation_state_->activation_level == ActivationLevel::kDryRun) {
    if (deferred_) {
      // Resume if allowed or we are in dry run mode.
      delegate_->Resume();
    }
  } else {
    // Cancel if the resource load should be blocked.
    subresource_disallowed = true;
    delegate_->CancelWithError(net::ERR_BLOCKED_BY_FINGERPRINTING_PROTECTION,
                               "FingerprintingProtection");
  }
  if (deferred_) {
    RecordDeferTimeHistogram(activation_state_->activation_level,
                             load_policy_.value(), defer_timestamp_);
  }
  deferred_ = false;

  if (filter_) {
    on_subresource_evaluated_callback_.Run(current_url_, devtools_request_id_,
                                           subresource_disallowed,
                                           filter_->statistics());
  }
}

}  // namespace fingerprinting_protection_filter
