// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PARENTAL_CONTROL_METRICS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PARENTAL_CONTROL_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"

namespace supervised_user {
class SupervisedUserURLFilter;

// A class for recording web filter metrics for Family Link users on Chrome
// browser at the beginning of the first active session daily.
class ParentalControlMetrics : public SupervisedUserMetricsService::Observer {
 public:
  explicit ParentalControlMetrics(
      supervised_user::SupervisedUserURLFilter* url_filter);
  ParentalControlMetrics(const ParentalControlMetrics&) = delete;
  ParentalControlMetrics& operator=(const ParentalControlMetrics&) = delete;
  ~ParentalControlMetrics() override;

  // SupervisedUserMetricsService::Observer:
  void OnNewDay() override;

 private:
  const raw_ptr<supervised_user::SupervisedUserURLFilter> url_filter_;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PARENTAL_CONTROL_METRICS_H_
