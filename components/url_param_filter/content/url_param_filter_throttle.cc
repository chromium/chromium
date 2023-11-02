// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_param_filter/content/url_param_filter_throttle.h"

#include <memory>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "components/url_param_filter/content/cross_otr_web_contents_observer.h"
#include "components/url_param_filter/core/features.h"
#include "components/url_param_filter/core/url_param_classifications_loader.h"
#include "components/url_param_filter/core/url_param_filterer.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"

namespace url_param_filter {
namespace {
// Write metrics about results of param filtering.
void WriteMetrics(FilterResult result) {
  // When experimental classifications are used, write a metric indicating this.
  // This allows validation of experimental results as being due to the
  // experiment vs filtering that would happen regardless.
  if (result.experimental_status ==
      ClassificationExperimentStatus::EXPERIMENTAL) {
    base::UmaHistogramCounts100(
        "Navigation.UrlParamFilter.FilteredParamCountExperimental",
        result.filtered_param_count);
  }
  base::UmaHistogramCounts100("Navigation.UrlParamFilter.FilteredParamCount",
                              result.filtered_param_count);
}
}  // anonymous namespace

void UrlParamFilterThrottle::MaybeCreateThrottle(
    bool enabled_by_policy,
    content::WebContents* web_contents,
    const network::ResourceRequest& request,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttle_list) {
  // If the enterprise escape hatch policy has been set, do not create the
  // throttle.
  if (!enabled_by_policy) {
    return;
  }
  // If we lack a web_contents, do not create the throttle.
  if (!web_contents) {
    return;
  }
  // Only outermost main frame navigations are in scope. We do not modify other
  // navigations.
  if (!request.is_outermost_main_frame) {
    return;
  }
  CrossOtrWebContentsObserver* observer =
      CrossOtrWebContentsObserver::FromWebContents(web_contents);
  if (observer && observer->IsCrossOtrState()) {
    throttle_list->push_back(std::make_unique<UrlParamFilterThrottle>(
        request.request_initiator, observer->GetWeakPtr()));
  }
}

UrlParamFilterThrottle::UrlParamFilterThrottle(
    const absl::optional<url::Origin>& request_initiator_origin,
    base::WeakPtr<CrossOtrWebContentsObserver> observer)
    : should_filter_(base::GetFieldTrialParamByFeatureAsBool(
          features::kIncognitoParamFilterEnabled,
          "should_filter",
          false)) {
  last_hop_initiator_ = request_initiator_origin.has_value()
                            ? request_initiator_origin->GetURL()
                            : GURL();
  observer_ = observer;
}
UrlParamFilterThrottle::~UrlParamFilterThrottle() = default;

void UrlParamFilterThrottle::DetachFromCurrentSequence() {}

void UrlParamFilterThrottle::WillStartRequest(network::ResourceRequest* request,
                                              bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  FilterResult result = FilterUrl(last_hop_initiator_, request->url,
                                  NestedFilterOption::kNoFilterNested);

  if (should_filter_) {
    request->url = result.filtered_url;
    WriteMetrics(result);
  }

  if (observer_ && result.filtered_param_count) {
    observer_->SetDidFilterParams(true, result.experimental_status);
  }
  last_hop_initiator_ = request->url;
}

void UrlParamFilterThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  FilterResult result = FilterUrl(last_hop_initiator_, redirect_info->new_url,
                                  NestedFilterOption::kNoFilterNested);

  if (should_filter_) {
    redirect_info->new_url = result.filtered_url;
    WriteMetrics(result);
  }

  if (observer_ && result.filtered_param_count) {
    observer_->SetDidFilterParams(true, result.experimental_status);
  }

  // Future redirects should use the redirect's domain as the navigation
  // source.
  last_hop_initiator_ = redirect_info->new_url;
}

bool UrlParamFilterThrottle::makes_unsafe_redirect() {
  // Scheme changes are not possible with this throttle. Only URL params are
  // modified.
  return false;
}
}  // namespace url_param_filter
