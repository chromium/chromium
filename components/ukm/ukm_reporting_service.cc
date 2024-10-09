// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ReportingService specialized to report UKM metrics.

#include "components/ukm/ukm_reporting_service.h"

#include <memory>
#include <string_view>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics/unsent_log_store.h"
#include "components/metrics/url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/ukm/ukm_service.h"
#include "components/ukm/unsent_log_store_metrics_impl.h"
#include "third_party/zlib/google/compression_utils.h"

#if BUILDFLAG(IS_IOS)
#include "components/ukm/ios/ukm_reporting_ios_util.h"
#endif

namespace ukm {

namespace {

// The number of UKM logs that will be stored in UnsentLogStore before logs
// start being dropped.
constexpr int kMinUnsentLogCount = 8;

// The number of bytes UKM logs that will be stored in UnsentLogStore before
// logs start being dropped.
// This ensures that a reasonable amount of history will be stored even if there
// is a long series of very small logs.
constexpr int kMinUnsentLogBytes = 300000;

// If an upload fails, and the transmission was over this byte count, then we
// will discard the log, and not try to retransmit it.  We also don't persist
// the log to the prefs for transmission during the next chrome session if this
// limit is exceeded.
constexpr size_t kMaxLogRetransmitSize = 100 * 1024;

GURL GetServerUrl() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(metrics::switches::kUkmServerUrl)) {
    return GURL(
        command_line->GetSwitchValueASCII(metrics::switches::kUkmServerUrl));
  }

  std::string server_url =
      base::GetFieldTrialParamValueByFeature(kUkmFeature, "ServerUrl");
  if (!server_url.empty())
    return GURL(server_url);
  return GURL(metrics::kDefaultUkmServerUrl);
}

}  // namespace

// static
void UkmReportingService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kUkmUnsentLogStore);
  // Base class already registered by MetricsReportingService::RegisterPrefs
  // ReportingService::RegisterPrefs(registry);
}

UkmReportingService::UkmReportingService(metrics::MetricsServiceClient* client,
                                         PrefService* local_state)
    : ReportingService(client,
                       local_state,
                       kMaxLogRetransmitSize,
                       /*logs_event_manager=*/nullptr),
      unsent_log_store_(std::make_unique<ukm::UnsentLogStoreMetricsImpl>(),
                        local_state,
                        prefs::kUkmUnsentLogStore,
                        nullptr,
                        metrics::UnsentLogStore::UnsentLogStoreLimits{
                            .min_log_count = kMinUnsentLogCount,
                            .min_queue_size_bytes = kMinUnsentLogBytes,
                            .max_log_size_bytes = kMaxLogRetransmitSize,
                        },
                        client->GetUploadSigningKey(),
                        /*logs_event_manager=*/nullptr) {}

UkmReportingService::~UkmReportingService() = default;

metrics::LogStore* UkmReportingService::log_store() {
  return &unsent_log_store_;
}

GURL UkmReportingService::GetUploadUrl() const {
  return GetServerUrl();
}

GURL UkmReportingService::GetInsecureUploadUrl() const {
  return GURL();
}

std::string_view UkmReportingService::upload_mime_type() const {
  return metrics::kUkmMimeType;
}

metrics::MetricsLogUploader::MetricServiceType
UkmReportingService::service_type() const {
  return metrics::MetricsLogUploader::UKM;
}

void UkmReportingService::LogCellularConstraint(bool upload_canceled) {
  UMA_HISTOGRAM_BOOLEAN("UKM.LogUpload.Canceled.CellularConstraint",
                        upload_canceled);
}

void UkmReportingService::LogResponseOrErrorCode(int response_code,
                                                 int error_code,
                                                 bool was_https) {
  // |was_https| is ignored since all UKM logs are received over HTTPS.
  base::UmaHistogramSparse("UKM.LogUpload.ResponseOrErrorCode",
                           response_code >= 0 ? response_code : error_code);
}

void UkmReportingService::LogSuccessLogSize(size_t log_size) {
#if BUILDFLAG(IS_IOS)
  IncrementUkmLogSizeOnSuccessCounter();
#endif
  UMA_HISTOGRAM_COUNTS_10000("UKM.LogSize.OnSuccess", log_size / 1024);
}

void UkmReportingService::LogSuccessMetadata(const std::string& staged_log) {
  // Recover the report from the compressed staged log.
  // Note: We don't use metrics::DecodeLogDataToProto() since we to use
  // |uncompressed_log_data| later in the function.
  std::string uncompressed_log_data;
  bool uncompress_successful =
      compression::GzipUncompress(staged_log, &uncompressed_log_data);
  DCHECK(uncompress_successful);
  Report report;
  report.ParseFromString(uncompressed_log_data);

  // Log the relative size of the report with relevant UKM data omitted. This
  // helps us to estimate the bandwidth usage of logs upload that is not
  // directly attributed to UKM data, for example the system profile info.
  // Note that serialized logs are further compressed before upload, thus the
  // percentages here are not the exact percentage of bandwidth they ended up
  // taking.
  std::string log_without_ukm_data;
  report.clear_sources();
  report.clear_source_counts();
  report.clear_entries();
  report.clear_aggregates();
  report.SerializeToString(&log_without_ukm_data);

  int non_ukm_percentage =
      log_without_ukm_data.length() * 100 / uncompressed_log_data.length();
  DCHECK_GE(non_ukm_percentage, 0);
  DCHECK_LE(non_ukm_percentage, 100);
  base::UmaHistogramPercentage("UKM.ReportSize.NonUkmPercentage",
                               non_ukm_percentage);
}

void UkmReportingService::LogLargeRejection(size_t log_size) {}

}  // namespace ukm
