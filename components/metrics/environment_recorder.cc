// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/environment_recorder.h"

#include "base/base64.h"
#include "base/hash/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

namespace {

// Computes a SHA-1 hash of |data| and returns it as a hex string.
std::string ComputeSHA1(const std::string& data) {
  return base::HexEncode(base::SHA1Hash(base::as_byte_span(data)));
}

}  // namespace

EnvironmentRecorder::EnvironmentRecorder(PrefService* local_state)
    : local_state_(local_state) {}

EnvironmentRecorder::~EnvironmentRecorder() = default;

std::string EnvironmentRecorder::SerializeAndRecordEnvironmentToPrefs(
    const SystemProfileProto& system_profile) {
  std::string serialized_system_profile;
  if (system_profile.SerializeToString(&serialized_system_profile)) {
    // Persist the system profile to disk. In the event of an unclean shutdown,
    // it will be used as part of the initial stability report.
    const std::string base64_system_profile =
        base::Base64Encode(serialized_system_profile);
    local_state_->SetString(prefs::kStabilitySavedSystemProfile,
                            base64_system_profile);
    local_state_->SetString(prefs::kStabilitySavedSystemProfileHash,
                            ComputeSHA1(serialized_system_profile));
  }

  return serialized_system_profile;
}

bool EnvironmentRecorder::LoadEnvironmentFromPrefs(
    SystemProfileProto* system_profile) {
  DCHECK(system_profile);

  const std::string base64_system_profile =
      local_state_->GetString(prefs::kStabilitySavedSystemProfile);
  if (base64_system_profile.empty())
    return false;
  const std::string system_profile_hash =
      local_state_->GetString(prefs::kStabilitySavedSystemProfileHash);

  std::string serialized_system_profile;
  return base::Base64Decode(base64_system_profile,
                            &serialized_system_profile) &&
         ComputeSHA1(serialized_system_profile) == system_profile_hash &&
         system_profile->ParseFromString(serialized_system_profile);
}

void EnvironmentRecorder::ClearEnvironmentFromPrefs() {
  local_state_->ClearPref(prefs::kStabilitySavedSystemProfile);
  local_state_->ClearPref(prefs::kStabilitySavedSystemProfileHash);
}

int64_t EnvironmentRecorder::GetLastBuildtime() {
  return local_state_->GetInt64(prefs::kStabilityStatsBuildTime);
}

std::string EnvironmentRecorder::GetLastVersion() {
  return local_state_->GetString(prefs::kStabilityStatsVersion);
}

void EnvironmentRecorder::SetBuildtimeAndVersion(int64_t buildtime,
                                                 const std::string& version) {
  local_state_->SetInt64(prefs::kStabilityStatsBuildTime, buildtime);
  local_state_->SetString(prefs::kStabilityStatsVersion, version);
}

// static
void EnvironmentRecorder::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kStabilitySavedSystemProfile,
                               std::string());
  registry->RegisterStringPref(prefs::kStabilitySavedSystemProfileHash,
                               std::string());
  registry->RegisterStringPref(prefs::kStabilityStatsVersion, std::string());
  registry->RegisterInt64Pref(prefs::kStabilityStatsBuildTime, 0);
}

}  // namespace metrics
