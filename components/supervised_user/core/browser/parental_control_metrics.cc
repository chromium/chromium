// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/parental_control_metrics.h"

#include "base/check.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/supervised_user_utils.h"

namespace supervised_user {

ParentalControlMetrics::ParentalControlMetrics(
    supervised_user::SupervisedUserURLFilter* url_filter)
    : url_filter_(url_filter) {
  DCHECK(url_filter);
}

ParentalControlMetrics::~ParentalControlMetrics() = default;

void ParentalControlMetrics::OnNewDay() {
  url_filter_->ReportManagedSiteListMetrics();
  url_filter_->ReportWebFilterTypeMetrics();
}

}  // namespace supervised_user
