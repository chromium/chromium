// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_METRICS_RECORDER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_METRICS_RECORDER_H_

#include <optional>

#include "base/memory/raw_ref.h"
#include "components/signin/public/base/signin_prefs.h"
#include "google_apis/gaia/gaia_id.h"

class PrefService;

namespace metrics {
class ProfileMetricsService;
}

namespace signin {

class IdentityManager;
struct AccountPreviewData;

class AccountPreviewMetricsRecorder {
 public:
  AccountPreviewMetricsRecorder(
      PrefService& pref_service,
      const IdentityManager& identity_manager,
      const metrics::ProfileMetricsService& profile_metrics_service);

  AccountPreviewMetricsRecorder(const AccountPreviewMetricsRecorder&) = delete;
  AccountPreviewMetricsRecorder& operator=(
      const AccountPreviewMetricsRecorder&) = delete;

  ~AccountPreviewMetricsRecorder();

  void RecordMetrics(const GaiaId& gaia_id, const AccountPreviewData& data);

 private:
  SigninPrefs signin_prefs_;
  raw_ref<const IdentityManager> identity_manager_;
  raw_ref<const metrics::ProfileMetricsService> profile_metrics_service_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_PREVIEW_METRICS_RECORDER_H_
