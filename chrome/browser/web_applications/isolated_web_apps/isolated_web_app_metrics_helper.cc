// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_metrics_helper.h"

#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/ukm/iwa_source_url_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace web_app {

// static
void IsolatedWebAppMetricsHelper::ReportNumInstalledSubApps(
    const WebAppRegistrar& registrar) {
  absl::flat_hash_map<webapps::AppId, int> iwa_id_to_sub_apps_count;

  for (const WebApp& app :
       registrar.GetApps(WebAppFilter::IsIsolatedSubApp())) {
    auto parent_app_id = app.parent_app_id();
    // If app matches WebAppFilter::IsIsolatedSubApp then parent_app_id
    // must be always defined.
    CHECK(parent_app_id);
    iwa_id_to_sub_apps_count[*parent_app_id]++;
  }

  for (const auto& [app_id, count] : iwa_id_to_sub_apps_count) {
    url::Origin origin = url::Origin::Create(registrar.GetAppStartUrl(app_id));
    ukm::SourceId source_id =
        ukm::IwaSourceUrlRecorder::GetSourceIdForIwaUrl(origin.GetURL());

    ukm::builders::SubApp_CountPerParent(source_id)
        .SetSubAppsCount(count)
        .Record(ukm::UkmRecorder::Get());
    ukm::IwaSourceUrlRecorder::MarkSourceForDeletion(source_id);
  }
}

// static
void IsolatedWebAppMetricsHelper::ReportSubAppInstallResults(
    const url::Origin& parent_app_origin,
    const std::vector<LogSubAppInstallResult>& results) {
  ukm::SourceId source_id = ukm::IwaSourceUrlRecorder::GetSourceIdForIwaUrl(
      parent_app_origin.GetURL());

  for (auto result : results) {
    ukm::builders::SubApp_InstallResult(source_id)
        .SetResult(static_cast<int>(result))
        .Record(ukm::UkmRecorder::Get());
  }

  ukm::IwaSourceUrlRecorder::MarkSourceForDeletion(source_id);
}

// static
void IsolatedWebAppMetricsHelper::ReportSubAppSilentUpdateResult(
    const url::Origin& parent_app_origin,
    ManifestSilentUpdateCheckResult result) {
  ukm::SourceId source_id = ukm::IwaSourceUrlRecorder::GetSourceIdForIwaUrl(
      parent_app_origin.GetURL());

  ukm::builders::SubApp_Update_ManifestSilentUpdateCheckResult(source_id)
      .SetResult(static_cast<int>(result))
      .Record(ukm::UkmRecorder::Get());

  ukm::IwaSourceUrlRecorder::MarkSourceForDeletion(source_id);
}

// static
void IsolatedWebAppMetricsHelper::ReportSubAppPendingUpdateResult(
    const url::Origin& parent_app_origin,
    ApplyPendingManifestUpdateResult result) {
  ukm::SourceId source_id = ukm::IwaSourceUrlRecorder::GetSourceIdForIwaUrl(
      parent_app_origin.GetURL());

  ukm::builders::SubApp_Update_ApplyPendingManifestUpdateResult(source_id)
      .SetResult(static_cast<int>(result))
      .Record(ukm::UkmRecorder::Get());

  ukm::IwaSourceUrlRecorder::MarkSourceForDeletion(source_id);
}

}  // namespace web_app
