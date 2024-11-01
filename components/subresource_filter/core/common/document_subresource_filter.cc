// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/document_subresource_filter.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/subresource_filter/core/common/first_party_origin.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

DocumentSubresourceFilter::DocumentSubresourceFilter(
    url::Origin document_origin,
    mojom::ActivationState activation_state,
    scoped_refptr<const MemoryMappedRuleset> ruleset,
    std::string_view uma_tag)
    : activation_state_(activation_state),
      ruleset_(std::move(ruleset)),
      ruleset_matcher_(ruleset_->data()),
      uma_tag_(uma_tag) {
  CHECK_NE(activation_state_.activation_level,
           mojom::ActivationLevel::kDisabled, base::NotFatalUntil::M129);
  if (!activation_state_.filtering_disabled_for_document) {
    document_origin_ =
        std::make_unique<FirstPartyOrigin>(std::move(document_origin));
  }
}

DocumentSubresourceFilter::~DocumentSubresourceFilter() = default;

LoadPolicy DocumentSubresourceFilter::GetLoadPolicy(
    const GURL& subresource_url,
    url_pattern_index::proto::ElementType subresource_type) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("loading"),
               "DocumentSubresourceFilter::GetLoadPolicy", "url",
               subresource_url.spec());

  ++statistics_.num_loads_total;

  if (activation_state_.filtering_disabled_for_document)
    return LoadPolicy::ALLOW;
  if (subresource_url.SchemeIs(url::kDataScheme))
    return LoadPolicy::ALLOW;

  // If ThreadTicks is not supported, then no CPU time measurements have been
  // collected. Don't report both CPU and wall duration to be consistent.
  auto wall_duration_timer = ScopedTimers::StartIf(
      activation_state_.measure_performance &&
          ScopedThreadTimers::IsSupported(),
      [this](base::TimeDelta delta) {
        statistics_.evaluation_total_wall_duration += delta;
        // Here we use custom histograms to maintain subresource filter
        // metrics, which used UMA_HISTOGRAM_MICRO_TIMES prior to introducing
        // support for fingerprinting protection filter counterpart.
        UmaHistogramCustomMicrosecondsTimes(
            base::StrCat(
                {uma_tag_, ".SubresourceLoad.Evaluation.WallDuration"}),
            delta, base::Microseconds(1), base::Seconds(1), 50);
      });
  auto cpu_duration_timer = ScopedThreadTimers::StartIf(
      activation_state_.measure_performance, [this](base::TimeDelta delta) {
        statistics_.evaluation_total_cpu_duration += delta;
        UmaHistogramCustomMicrosecondsTimes(
            base::StrCat({uma_tag_, ".SubresourceLoad.Evaluation.CPUDuration"}),
            delta, base::Microseconds(1), base::Seconds(1), 50);
      });

  ++statistics_.num_loads_evaluated;
  CHECK(document_origin_, base::NotFatalUntil::M129);
  LoadPolicy result = ruleset_matcher_.GetLoadPolicyForResourceLoad(
      subresource_url, *document_origin_, subresource_type,
      activation_state_.generic_blocking_rules_disabled);
  CHECK_NE(LoadPolicy::WOULD_DISALLOW, result, base::NotFatalUntil::M129);
  if (result == LoadPolicy::DISALLOW) {
    ++statistics_.num_loads_matching_rules;
    if (activation_state_.activation_level ==
        mojom::ActivationLevel::kEnabled) {
      ++statistics_.num_loads_disallowed;
      return LoadPolicy::DISALLOW;
    } else if (activation_state_.activation_level ==
               mojom::ActivationLevel::kDryRun) {
      return LoadPolicy::WOULD_DISALLOW;
    }
  }
  return result;
}

const url_pattern_index::flat::UrlRule*
DocumentSubresourceFilter::FindMatchingUrlRule(
    const GURL& subresource_url,
    url_pattern_index::proto::ElementType subresource_type) {
  if (activation_state_.filtering_disabled_for_document)
    return nullptr;
  if (subresource_url.SchemeIs(url::kDataScheme))
    return nullptr;

  return ruleset_matcher_.MatchedUrlRule(
      subresource_url, *document_origin_, subresource_type,
      activation_state_.generic_blocking_rules_disabled);
}

}  // namespace subresource_filter
