// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_param_filter/core/cross_otr_observer.h"
#include "base/metrics/histogram_functions.h"
#include "net/http/http_util.h"

namespace url_param_filter {

CrossOtrObserver::CrossOtrObserver(ObserverType observer_type)
    : observer_type_(observer_type) {}

void CrossOtrObserver::WriteRefreshMetric() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If we used experimental classifications, write the experimental metric in
  // addition to the standard one for additional segmentation (default vs
  // experimental).
  if (experiment_status_ == ClassificationExperimentStatus::EXPERIMENTAL) {
    base::UmaHistogramCounts100(
        "Navigation.CrossOtr.ContextMenu.RefreshCountExperimental",
        refresh_count_);
  }
  base::UmaHistogramCounts100("Navigation.CrossOtr.ContextMenu.RefreshCount",
                              refresh_count_);
}
void CrossOtrObserver::WriteResponseMetric(int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If we used experimental classifications, write the experimental metric in
  // addition to the standard one for additional segmentation (default vs
  // experimental).
  if (experiment_status_ == ClassificationExperimentStatus::EXPERIMENTAL) {
    base::UmaHistogramSparse(
        "Navigation.CrossOtr.ContextMenu.ResponseCodeExperimental",
        response_code);
  }
  base::UmaHistogramSparse("Navigation.CrossOtr.ContextMenu.ResponseCode",
                           response_code);
}

void CrossOtrObserver::OnNavigationStart(absl::optional<bool> is_primary_frame,
                                         absl::optional<bool> user_activated,
                                         bool is_client_redirect,
                                         absl::optional<bool> init_cross_otr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observer_type_ == ObserverType::kIos) {
    // The observer on iOS doesn't get initialized the same way as on content/
    // so we need to check and store whether we are entering cross-OTR on the
    // first navigation.
    if (!observed_response_ && init_cross_otr.has_value()) {
      protecting_navigations_ = init_cross_otr.value();
      return;
    }
  }
  // If we've already observed the end of a navigation, and the navigation is
  // in the primary main frame, and it is not the result of a client redirect,
  // we've finished the cross-OTR case. Note that observing user activation
  // would also serve to stop the protecting_navigations_ case. Note that
  // refreshes after page load also trigger this, and thus are not at risk of
  // being considered part of the cross-OTR case.
  bool navigation_is_primary_frame =
      is_primary_frame.has_value() && is_primary_frame.value();
  bool navigation_is_user_activated =
      user_activated.has_value() && user_activated.value();
  if (observed_response_) {
    if ((!is_client_redirect && navigation_is_primary_frame) ||
        (!is_client_redirect || navigation_is_user_activated)) {
      protecting_navigations_ = false;
    }
  }
}

bool CrossOtrObserver::OnNavigationFinish(
    absl::optional<bool> is_primary_frame,
    bool is_same_document,
    const net::HttpResponseHeaders* headers,
    bool is_reload,
    bool has_committed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_same_document ||
      (is_primary_frame.has_value() && !is_primary_frame.value())) {
    // We only are concerned with top-level, non-same doc navigations.
    return false;
  }
  // We only want the first navigation, including client redirects occurring
  // without having observed user activation, to be counted; after that, no
  // response codes should be tracked. The observer is left in place to track
  // refreshes on the first page.
  if (protecting_navigations_) {
    observed_response_ = true;
    if (headers && did_filter_params_) {
      WriteResponseMetric(
          net::HttpUtil::MapStatusCodeForHistogram(headers->response_code()));
    }
    return false;
  }
  if (is_reload) {
    refresh_count_++;
    return false;
  }
  if (has_committed && !protecting_navigations_) {
    // Derived observers should detach in this case.
    return true;
  }
  return false;
}

void CrossOtrObserver::OnNavigationRedirect(
    bool is_primary_frame,
    bool is_same_document,
    const net::HttpResponseHeaders* headers,
    bool is_internal_redirect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_primary_frame || is_same_document) {
    // We only are concerned with top-level, non-same doc navigations.
    return;
  }

  // After the first full navigation has committed, including any client
  // redirects that occur without user activation, we no longer want to track
  // redirects.
  // Metrics will not be collected for non intervened navigation chains and
  // navigations occurring prior to params filtering.
  if (protecting_navigations_ && headers && did_filter_params_ &&
      !is_internal_redirect) {
    WriteResponseMetric(
        net::HttpUtil::MapStatusCodeForHistogram(headers->response_code()));
  }
}

void CrossOtrObserver::SetDidFilterParams(
    bool value,
    ClassificationExperimentStatus experiment_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  did_filter_params_ = value;
  // If we have already seen experimental params, treat all metrics as coming
  // after an experimental param classification. In other words, we consider
  // all response codes/refresh counts after an experimental param has been
  // stripped as being influenced by that experimental parameter removal.
  if (experiment_status_ != ClassificationExperimentStatus::EXPERIMENTAL) {
    experiment_status_ = experiment_status;
  }
}

}  // namespace url_param_filter
