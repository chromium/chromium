// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"

#include <fstream>
#include <ios>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_parser.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  // If attestations aren't enabled, pass the check trivially.
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kEnforcePrivacySandboxAttestations)) {
    return PrivacySandboxSettingsImpl::Status::kAllowed;
  }

  // Pass the check if the site is in the list of devtools overrides.
  if (IsOverridden(site)) {
    return PrivacySandboxSettingsImpl::Status::kAllowed;
  }

  // When the attestations map is not present, the behavior is default-deny.
  if (!attestations_map_.has_value()) {
    // Break down by type of failure.

    // If parsing hasn't started, the attestations file hasn't been downloaded,
    // or this is a fresh boot and the component hasn't checked the filesystem
    // yet.
    if (attestations_parse_progress_ == Progress::kNotStarted) {
      return PrivacySandboxSettingsImpl::Status::kAttestationsFileNotYetReady;
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

void PrivacySandboxAttestations::LoadAttestations(base::Version version,
                                                  base::FilePath install_dir) {
  // This function should only be called when the feature is enabled.
  CHECK(base::FeatureList::IsEnabled(
      privacy_sandbox::kEnforcePrivacySandboxAttestations));
  CHECK(version.IsValid());

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PrivacySandboxAttestations::LoadAttestationsInternal,
                     base::Unretained(this), std::move(version),
                     std::move(install_dir)));
}

void PrivacySandboxAttestations::AddOverride(const net::SchemefulSite& site) {
  overridden_sites_.push_back(site);
}

bool PrivacySandboxAttestations::IsOverridden(
    const net::SchemefulSite& site) const {
  return IsOverriddenByFlags(site) || base::Contains(overridden_sites_, site);
}

void PrivacySandboxAttestations::SetAttestationsForTesting(
    absl::optional<PrivacySandboxAttestationsMap> attestations_map) {
  attestations_map_ = std::move(attestations_map);
}

base::Version PrivacySandboxAttestations::GetVersionForTesting() const {
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

PrivacySandboxAttestations::PrivacySandboxAttestations()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {}

void PrivacySandboxAttestations::LoadAttestationsInternal(
    base::Version version,
    base::FilePath install_dir) {
  // This function should only be called when the feature is enabled.
  CHECK(base::FeatureList::IsEnabled(
      privacy_sandbox::kEnforcePrivacySandboxAttestations));
  CHECK(version.IsValid());

  if (!file_version_.IsValid()) {
    // There is no existing attestations map.
    CHECK(!attestations_map_.has_value());
    CHECK_EQ(attestations_parse_progress_, Progress::kNotStarted);
  } else {
    // There is an existing attestations map.
    CHECK(attestations_map_.has_value());
    // The progress should be `kFinished` because this function is always
    // executed on the same SequencedTaskRunner `task_runner_`.
    CHECK_EQ(attestations_parse_progress_, Progress::kFinished);

    if (file_version_.CompareTo(version) >= 0) {
      // The existing attestations map is of newer or same version, do not
      // parse.
      RunLoadAttestationsDoneCallbackForTesting();  // IN-TEST
      return;
    }
  }

  attestations_parse_progress_ = Progress::kStarted;

  std::ifstream stream(install_dir.AsUTF8Unsafe(),
                       std::ios::binary | std::ios::in);
  if (!stream.is_open()) {
    // File does not exist.
    attestations_parse_progress_ = Progress::kFinished;
    RunLoadAttestationsDoneCallbackForTesting();  // IN-TEST
    return;
  }

  if (RunLoadAttestationsParsingStartedCallbackForTesting()) {  // IN-TEST
    // If necessary for testing, indefinitely pause parsing once it's started.
    return;
  }

  base::ElapsedTimer parsing_timer;
  absl::optional<PrivacySandboxAttestationsMap> attestations_map =
      ParseAttestationsFromStream(stream);
  if (!attestations_map.has_value()) {
    // The parsing failed.
    attestations_parse_progress_ = Progress::kFinished;
    RunLoadAttestationsDoneCallbackForTesting();  // IN-TEST
    return;
  }
  // For an attestations file with 10,000 entries, the average parsing time is
  // around 150 microsecond.
  base::UmaHistogramMicrosecondsTimes(kAttestationsFileParsingUMA,
                                      parsing_timer.Elapsed());

  // Queries on Privacy Sandbox APIs attestation status may happen on the UI
  // thread. The final assignment of the attestations map and its version is
  // done on the UI thread to avoid race condition.
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&PrivacySandboxAttestations::SetParsedAttestations,
                         base::Unretained(this), std::move(version),
                         std::move(attestations_map.value())));
}

void PrivacySandboxAttestations::SetParsedAttestations(
    base::Version version,
    PrivacySandboxAttestationsMap attestations_map) {
  file_version_ = std::move(version);
  attestations_map_ = std::move(attestations_map);
  attestations_parse_progress_ = Progress::kFinished;

  RunLoadAttestationsDoneCallbackForTesting();  // IN-TEST
}

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

}  // namespace privacy_sandbox
