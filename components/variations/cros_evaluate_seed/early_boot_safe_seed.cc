// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/early_boot_safe_seed.h"

#include "base/time/time.h"

namespace variations::cros_early_boot::evaluate_seed {

EarlyBootSafeSeed::EarlyBootSafeSeed(
    const featured::SeedDetails& safe_seed_details)
    : safe_seed_details_(safe_seed_details) {}

EarlyBootSafeSeed::~EarlyBootSafeSeed() = default;

base::Time EarlyBootSafeSeed::GetFetchTime() const {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Milliseconds(safe_seed_details_.fetch_time()));
}

void EarlyBootSafeSeed::SetFetchTime(const base::Time& fetch_time) {}

int EarlyBootSafeSeed::GetMilestone() const {
  return safe_seed_details_.milestone();
}

void EarlyBootSafeSeed::SetMilestone(int milestone) {}

base::Time EarlyBootSafeSeed::GetTimeForStudyDateChecks() const {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Milliseconds(safe_seed_details_.date()));
}

void EarlyBootSafeSeed::SetTimeForStudyDateChecks(
    const base::Time& safe_seed_time) {}

std::string EarlyBootSafeSeed::GetCompressedSeed() const {
  return safe_seed_details_.b64_compressed_data();
}

void EarlyBootSafeSeed::SetCompressedSeed(const std::string& safe_compressed) {}

std::string EarlyBootSafeSeed::GetSignature() const {
  return safe_seed_details_.signature();
}

void EarlyBootSafeSeed::SetSignature(const std::string& safe_seed_signature) {}

std::string EarlyBootSafeSeed::GetLocale() const {
  return safe_seed_details_.locale();
}

void EarlyBootSafeSeed::SetLocale(const std::string& locale) {}

std::string EarlyBootSafeSeed::GetPermanentConsistencyCountry() const {
  return safe_seed_details_.permanent_consistency_country();
}
void EarlyBootSafeSeed::SetPermanentConsistencyCountry(
    const std::string& permanent_consistency_country) {}

std::string EarlyBootSafeSeed::GetSessionConsistencyCountry() const {
  return safe_seed_details_.session_consistency_country();
}

void EarlyBootSafeSeed::SetSessionConsistencyCountry(
    const std::string& session_consistency_country) {}

void EarlyBootSafeSeed::ClearState() {}

}  // namespace variations::cros_early_boot::evaluate_seed
