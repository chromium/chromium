// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/parental_control_metrics.h"

#include "base/check.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/supervised_user_utils.h"

namespace supervised_user {

ParentalControlMetrics::ParentalControlMetrics(
    PrefService* user_prefs,
    supervised_user::SupervisedUserURLFilter* url_filter)
    : user_prefs_(user_prefs), url_filter_(url_filter) {
  DCHECK(url_filter);
}

ParentalControlMetrics::~ParentalControlMetrics() = default;

void ParentalControlMetrics::OnNewDay() {
  // Ignores reports when web filter prefs are reset to default value. It
  // might happen during sign out.
  if (supervised_user::AreWebFilterPrefsDefault(*user_prefs_)) {
    return;
  }
  url_filter_->ReportManagedSiteListMetrics();
  url_filter_->ReportWebFilterTypeMetrics();
}

}  // namespace supervised_user
