// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PARAM_FILTER_CORE_CROSS_OTR_OBSERVER_H_
#define COMPONENTS_URL_PARAM_FILTER_CORE_CROSS_OTR_OBSERVER_H_

#include "base/sequence_checker.h"
#include "components/url_param_filter/core/url_param_filterer.h"
#include "net/http/http_response_headers.h"

namespace url_param_filter {

enum class ObserverType { kContent, kIos };

// Observes navigations that originate in normal browsing and move into OTR
// browsing. This class can be thought of as a state machine:
// start-->blocking-->monitoring-->detached
// where the initial cross-OTR navigation moves to blocking; user activation or
// the start of a second navigation not initiated via client redirect moves to
// monitoring; and the next completed non-refresh navigation after that point,
// regardless of cause, detaches. Note that for our purposes, navigation above
// refers to top-level, main frame navigations only; we do not consider e.g.,
// subframe loads.
//
// This class handles the state-machine logic and transitions, leaving detaching
// to derived classes. This is the base class for CrossOtrWebContentsObserver
// (content/) and CrossOtrTabHelper (iOS).
class CrossOtrObserver {
 public:
  explicit CrossOtrObserver(ObserverType observer);
  ~CrossOtrObserver() = default;

  // Doesn't return anything since this can't lead to detaching of the state
  // machine.
  //
  // The optional parameters are due to some callers not having that
  // information.
  void OnNavigationStart(absl::optional<bool> is_primary_frame,
                         absl::optional<bool> user_activated,
                         bool is_client_redirect,
                         absl::optional<bool> init_cross_otr);

  // Doesn't return anything since this can't lead to detaching of the state
  // machine.
  void OnNavigationRedirect(bool is_primary_frame,
                            bool is_same_document,
                            const net::HttpResponseHeaders* headers,
                            bool is_internal_redirect);

  // Returns whether or not the observer should detach after this action is
  // observed.
  //
  // The optional parameters are due to some callers not having that
  // information.
  bool OnNavigationFinish(absl::optional<bool> is_primary_frame,
                          bool is_same_document,
                          const net::HttpResponseHeaders* headers,
                          bool is_reload,
                          bool has_committed);

  // Inform this observer that params were filtered, which means metrics should
  // be written. `experiment_status` indicates whether the parameters stripped
  // were based on experimental classifications.
  void SetDidFilterParams(bool value,
                          ClassificationExperimentStatus experiment_status);
  bool IsCrossOtrState() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return protecting_navigations_;
  }

 protected:
  // Writes response code metric(s) to monitor for potential breakge.
  void WriteResponseMetric(int response_code);

  // Writes refresh count metric(s) to monitor for potential breakage.
  void WriteRefreshMetric();

  // Exits this observer from cross-OTR state.
  void ExitCrossOtr() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    protecting_navigations_ = false;
  }

  bool did_filter_params() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return did_filter_params_;
  }

 private:
  // Drives state machine logic; we write the cross-OTR response code metric
  // only for the first navigation, which is that which would have parameters
  // filtered.
  bool observed_response_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Tracks whether params were filtered before the observer was created.
  bool did_filter_params_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Tracks refreshes observed, which could point to an issue with param
  // filtering causing unexpected behavior for the user.
  int refresh_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // Whether top-level navigations should have filtering applied. Starts true
  // then switches to false once a navigation completes and then either:
  // user interaction is observed or a new navigation starts that is not a
  // client redirect.
  bool protecting_navigations_ GUARDED_BY_CONTEXT(sequence_checker_) = true;

  // The type of filtering that occurred when entering the current webstate.
  // This can take on two values : EXPERIMENTAL or NON_EXPERIMENTAL.
  ClassificationExperimentStatus experiment_status_ GUARDED_BY_CONTEXT(
      sequence_checker_) = ClassificationExperimentStatus::NON_EXPERIMENTAL;

  // This can take on two values : kContent or kIos.
  ObserverType observer_type_;

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace url_param_filter

#endif  // COMPONENTS_URL_PARAM_FILTER_CORE_CROSS_OTR_OBSERVER_H_
