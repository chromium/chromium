// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"

#include <ios>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/types/expected.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_histograms.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_parser.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/privacy_sandbox_attestations_observer.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"

namespace privacy_sandbox {

namespace {

// Global PrivacySandboxAttestations instance for testing.
PrivacySandboxAttestations* g_test_instance = nullptr;

// Helper function that checks if enrollment overrides are set from the
// chrome://flags entry.
bool IsOverriddenByFlags(const net::SchemefulSite& site) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (!command_line.HasSwitch(
          privacy_sandbox::kPrivacySandboxEnrollmentOverrides)) {
    return false;
  }

  std::string origins_str = command_line.GetSwitchValueASCII(
      privacy_sandbox::kPrivacySandboxEnrollmentOverrides);

  std::vector<std::string> overrides_list = base::SplitString(
      origins_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (std::string override_str : overrides_list) {
    if (override_str.empty()) {
      continue;
    }

    GURL override_url(override_str);
    if (!override_url.is_valid()) {
      continue;
    }

    if (net::SchemefulSite(override_url) == site) {
      return true;
    }
  }

  return false;
}

void RecordParsingStatusHistogram(ParsingStatus status) {
  base::UmaHistogramEnumeration(kAttestationsFileParsingStatusUMA, status);
}

// Trigger the opening and parsing of the attestations file. Returns the
// parsed `attestations_map_` or the failure status. This function should only
// be invoked with `kEnforcePrivacySandboxAttestations` enabled.
// `installed_file_path` is the path to the attestations list file.
base::expected<PrivacySandboxAttestationsMap, ParsingStatus>
LoadAttestationsInternal(base::FilePath installed_file_path,
                         base::Version version) {
  // This function should only be called when the feature is enabled.
  CHECK(base::FeatureList::IsEnabled(
      privacy_sandbox::kEnforcePrivacySandboxAttestations));

  std::string proto_str;
  // When reading the file, the `base::FilePath` directory should be used to
  // make sure it works across platforms. If using the converted directory
  // returned by `base::FilePath::AsUTF8Unsafe()`, it fails on Windows when the
  // directory contains combining characters.
  if (!base::ReadFileToString(installed_file_path, &proto_str)) {
    return base::unexpected(ParsingStatus::kFileNotExist);
  }

  base::ElapsedTimer parsing_timer;
  std::optional<PrivacySandboxAttestationsMap> attestations_map =
      ParseAttestationsFromString(proto_str);
  if (!attestations_map.has_value()) {
    // The parsing failed.
    return base::unexpected(ParsingStatus::kCannotParseFile);
  }

  // For an attestations file with 10,000 entries, the average parsing time is
  // around 240 milliseconds as per local testing on a n2-standard-128 with 128
  // vCPUs and 512 GB memory. The estimated dynamic memory usage is around 880
  // KB.
  base::UmaHistogramTimes(kAttestationsFileParsingTimeUMA,
                          parsing_timer.Elapsed());
  // Count up to 10000 KB with a minimum of 1 KB.
  base::UmaHistogramCounts10000(
      kAttestationsMapMemoryUsageUMA,
      base::trace_event::EstimateMemoryUsage(attestations_map.value()) / 1024);

  return base::ok(std::move(attestations_map.value()));
}

}  // namespace

// static
PrivacySandboxAttestations* PrivacySandboxAttestations::GetInstance() {
  if (g_test_instance) {
    return g_test_instance;
  }

  static base::NoDestructor<PrivacySandboxAttestations> instance;
  return instance.get();
}

// static
void PrivacySandboxAttestations::SetInstanceForTesting(
    PrivacySandboxAttestations* test_instance) {
  g_test_instance = test_instance;
}

// static
std::unique_ptr<PrivacySandboxAttestations>
PrivacySandboxAttestations::CreateForTesting() {
  std::unique_ptr<PrivacySandboxAttestations> test_instance(
      new PrivacySandboxAttestations());
  return test_instance;
}

PrivacySandboxAttestations::~PrivacySandboxAttestations() = default;

PrivacySandboxSettingsImpl::Status PrivacySandboxAttestations::IsSiteAttested(
    const net::SchemefulSite& site,
    PrivacySandboxAttestationsGatedAPI invoking_api) const {
  PrivacySandboxSettingsImpl::Status status =
      IsSiteAttestedInternal(site, invoking_api);
  base::UmaHistogramEnumeration(kAttestationStatusUMA, status);

  // Record the time when the first Privacy Sandbox API attestation check takes
  // place.
  startup_metric_utils::GetBrowser().RecordPrivacySandboxAttestationFirstCheck(
      base::TimeTicks::Now());

  // If the attestations map is absent and feature
  // `kDefaultAllowPrivacySandboxAttestations` is on, default allow.
  switch (status) {
    case PrivacySandboxSettingsImpl::Status::kAttestationsFileNotPresent:
    case PrivacySandboxSettingsImpl::Status::
        kAttestationsDownloadedNotYetLoaded:
    case PrivacySandboxSettingsImpl::Status::kAttestationsFileCorrupt:
    case PrivacySandboxSettingsImpl::Status::kAttestationsFileNotYetChecked:
      return base::FeatureList::IsEnabled(
                 kDefaultAllowPrivacySandboxAttestations)
                 ? PrivacySandboxSettingsImpl::Status::kAllowed
                 : status;
    default: {
      // Record whether the attestation map is parsed from the pre-installed or
      // downloaded file.
      base::UmaHistogramEnumeration(kAttestationsFileSource,
                                    is_pre_installed()
                                        ? FileSource::kPreInstalled
                                        : FileSource::kDownloaded);
      return status;
    }
  }
}

PrivacySandboxSettingsImpl::Status
PrivacySandboxAttestations::IsSiteAttestedInternal(
    const net::SchemefulSite& site,
    PrivacySandboxAttestationsGatedAPI invoking_api) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If attestations aren't enabled, pass the check trivially.
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kEnforcePrivacySandboxAttestations)) {
    return PrivacySandboxSettingsImpl::Status::kAllowed;
  }

  // Test has marked all Privacy Sandbox APIs as attested for any given site.
  if (is_all_apis_attested_for_testing_) {
    return PrivacySandboxSettingsImpl::Status::kAllowed;
  }

  // Pass the check if the site is in the list of devtools overrides.
  if (IsOverridden(site)) {
    return PrivacySandboxSettingsImpl::Status::kAllowed;
  }

  // When the attestations map is not present, return the reason.
  if (!attestations_map_.has_value()) {
    // Break down by type of failure.

    // If parsing hasn't started, there are two possible cases:
    // 1. `kAttestationsFileNotPresent`: The component installer has already
    // checked and found the attestations file did not exist on disk.
    // 2. `kAttestationsFileNotYetChecked`: The component installer has not
    // checked the attestations file yet. The file may or may not exist on disk.
    if (attestations_parse_progress_ == Progress::kNotStarted) {
      return attestations_file_checked() ? PrivacySandboxSettingsImpl::Status::
                                               kAttestationsFileNotPresent
                                         : PrivacySandboxSettingsImpl::Status::
                                               kAttestationsFileNotYetChecked;
    }

    // If parsing is in progress, the attestation file has been downloaded but
    // isn't ready for use yet.
    if (attestations_parse_progress_ == Progress::kStarted) {
      return PrivacySandboxSettingsImpl::Status::
          kAttestationsDownloadedNotYetLoaded;
    }

    // If parsing is finished but there is still no attestations map, the
    // attestation file must have been corrupt.
    if (attestations_parse_progress_ == Progress::kFinished) {
      return PrivacySandboxSettingsImpl::Status::kAttestationsFileCorrupt;
    }
  }

  // If `site` isn't enrolled at all, fail the check.
  auto it = attestations_map_->find(site);
  if (it == attestations_map_->end()) {
    return PrivacySandboxSettingsImpl::Status::kAttestationFailed;
  }

  // If `site` is attested for `invoking_api`, pass the check.
  if (it->second.Has(invoking_api)) {
    return PrivacySandboxSettingsImpl::Status::kAllowed;
  }

  // Otherwise, fail.
  return PrivacySandboxSettingsImpl::Status::kAttestationFailed;
}

void PrivacySandboxAttestations::LoadAttestations(
    base::Version version,
    base::FilePath installed_file_path,
    bool is_pre_installed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `LoadAttestations` is invoked by `ComponentReady`, which is always run on
  // the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This function should only be called when the feature is enabled.
  CHECK(base::FeatureList::IsEnabled(
      privacy_sandbox::kEnforcePrivacySandboxAttestations));
  CHECK(version.IsValid());

  // File version and attestations map should either both exist, or neither of
  // them exist.
  CHECK(file_version_.IsValid() == attestations_map_.has_value());

  if (file_version_.IsValid() && file_version_.CompareTo(version) >= 0) {
    // No need to parse if the incoming version is not newer than the existing
    // one.
    RecordParsingStatusHistogram(ParsingStatus::kNotNewerVersion);
    RunLoadAttestationsDoneCallbackForTesting();  // IN-TEST
    return;
  }

  // Mark the progress as started before posting the parsing task to the
  // sequenced task runner.
  attestations_parse_progress_ = Progress::kStarted;

  if (RunLoadAttestationsParsingStartedCallbackForTesting()) {  // IN-TEST
    // If necessary for testing, indefinitely pause parsing once the progress
    // is marked as started.
    return;
  }

  // Post the parsing task to the sequenced task runner. Upon receiving the
  // parsed attestations map, store it to `attestations_map_` in callback
  // `OnAttestationsLoaded` on UI thread. `base::Unretained(this)` is fine in
  // the reply callback because this is a singleton that will never be
  // destroyed.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoadAttestationsInternal, std::move(installed_file_path),
                     version),
      base::BindOnce(&PrivacySandboxAttestations::OnAttestationsParsed,
                     base::Unretained(this), version, is_pre_installed));
}

void PrivacySandboxAttestations::AddOverride(const net::SchemefulSite& site) {
  overridden_sites_.push_back(site);
}

bool PrivacySandboxAttestations::IsOverridden(
    const net::SchemefulSite& site) const {
  return IsOverriddenByFlags(site) || base::Contains(overridden_sites_, site);
}

void PrivacySandboxAttestations::SetAllPrivacySandboxAttestedForTesting(
    bool all_attested) {
  is_all_apis_attested_for_testing_ = all_attested;
  NotifyObserversOnAttestationsLoaded();
}

void PrivacySandboxAttestations::SetAttestationsForTesting(
    std::optional<PrivacySandboxAttestationsMap> attestations_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  attestations_map_ = std::move(attestations_map);
  NotifyObserversOnAttestationsLoaded();
}

base::Version PrivacySandboxAttestations::GetVersionForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return file_version_;
}

void PrivacySandboxAttestations::SetLoadAttestationsDoneCallbackForTesting(
    base::OnceClosure callback) {
  load_attestations_done_callback_ = std::move(callback);
}

void PrivacySandboxAttestations::
    SetLoadAttestationsParsingStartedCallbackForTesting(
        base::OnceClosure callback) {
  load_attestations_parsing_started_callback_ = std::move(callback);
}

void PrivacySandboxAttestations::SetComponentRegistrationCallbackForTesting(
    base::OnceClosure callback) {
  component_registration_callback_ = std::move(callback);
}

PrivacySandboxAttestations::PrivacySandboxAttestations()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {}

void PrivacySandboxAttestations::RunLoadAttestationsDoneCallbackForTesting() {
  if (!load_attestations_done_callback_.is_null()) {
    std::move(load_attestations_done_callback_).Run();
  }
}

bool PrivacySandboxAttestations::
    RunLoadAttestationsParsingStartedCallbackForTesting() {
  if (!load_attestations_parsing_started_callback_.is_null()) {
    std::move(load_attestations_parsing_started_callback_).Run();
    return true;
  }
  return false;
}

void PrivacySandboxAttestations::RunComponentRegistrationCallbackForTesting() {
  if (component_registration_callback_) {
    std::move(component_registration_callback_).Run();
  }
}

void PrivacySandboxAttestations::OnAttestationsParsed(
    base::Version version,
    bool is_pre_installed,
    base::expected<PrivacySandboxAttestationsMap, ParsingStatus>
        attestations_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Queries on Privacy Sandbox APIs attestation status will happen on the UI
  // thread. The final assignment of the attestations map and its version is
  // done on the UI thread to avoid race condition.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  RecordParsingStatusHistogram(attestations_map.has_value()
                                   ? ParsingStatus::kSuccess
                                   : attestations_map.error());

  if (attestations_map.has_value() &&
      (!file_version_.IsValid() || file_version_.CompareTo(version) < 0)) {
    // Parsing succeeded and the attestations file has newer version.
    file_version_ = std::move(version);
    attestations_map_ = std::move(attestations_map.value());
  }

  SetIsPreInstalled(is_pre_installed);
  attestations_parse_progress_ = Progress::kFinished;

  // Do not remove. There is an internal test that depends on the loggings.
  VLOG(1) << "Parsed Privacy Sandbox Attestation list version: "
          << file_version_;
  VLOG(1) << "Number of attestation entries: "
          << (attestations_map_ ? attestations_map_->size() : 0);

  NotifyObserversOnAttestationsLoaded();

  RunLoadAttestationsDoneCallbackForTesting();  // IN-TEST
}

void PrivacySandboxAttestations::NotifyObserversOnAttestationsLoaded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& observer : observers_) {
    observer.OnAttestationsLoaded();
  }
}

bool PrivacySandboxAttestations::AddObserver(
    content::PrivacySandboxAttestationsObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // When the feature is disabled, the attestations are not enforced and the
  // attestations are not loaded. Returning true so that the observers don't
  // have to wait indefinitely.
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kEnforcePrivacySandboxAttestations)) {
    return true;
  }

  observers_.AddObserver(observer);

  return IsEverLoaded();
}

void PrivacySandboxAttestations::RemoveObserver(
    content::PrivacySandboxAttestationsObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  observers_.RemoveObserver(observer);
}

bool PrivacySandboxAttestations::IsEverLoaded() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40287460): Add lock to `attestations_parse_progress_`.
  return attestations_map_.has_value() ||
         attestations_parse_progress_ == Progress::kFinished ||
         is_all_apis_attested_for_testing_;
}

void PrivacySandboxAttestations::OnAttestationsFileCheckComplete() {
  attestations_file_checked_ = true;
  RunComponentRegistrationCallbackForTesting();  // IN-TEST
}

}  // namespace privacy_sandbox
