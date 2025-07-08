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
  return GetCompressedSeed().client_fetch_time;
}

void EarlyBootSafeSeed::SetFetchTime(const base::Time& fetch_time) {}

int EarlyBootSafeSeed::GetMilestone() const {
  return GetCompressedSeed().milestone;
}

base::Time EarlyBootSafeSeed::GetTimeForStudyDateChecks() const {
  return GetCompressedSeed().seed_date;
}

StoredSeed EarlyBootSafeSeed::GetCompressedSeed() const {
  return StoredSeed(
      /*storage_format=*/StoredSeed::StorageFormat::kCompressedAndBase64Encoded,
      /*data=*/safe_seed_details_.b64_compressed_data(),
      /*signature=*/safe_seed_details_.signature(),
      /*milestone=*/safe_seed_details_.milestone(),
      /*seed_date=*/
      base::Time::FromDeltaSinceWindowsEpoch(
          base::Milliseconds(safe_seed_details_.date())),
      /*client_fetch_time=*/
      base::Time::FromDeltaSinceWindowsEpoch(
          base::Milliseconds(safe_seed_details_.fetch_time())),
      /*session_country_code=*/
      safe_seed_details_.session_consistency_country(),
      /*permanent_country_code=*/
      safe_seed_details_.permanent_consistency_country(),
      // Permanent version is not stored in the safe seed, only the country.
      /*permanent_country_version=*/"");
}

void EarlyBootSafeSeed::SetCompressedSeed(ValidatedSeedInfo seed_info) {}

std::string EarlyBootSafeSeed::GetLocale() const {
  return safe_seed_details_.locale();
}

void EarlyBootSafeSeed::SetLocale(const std::string& locale) {}

std::string EarlyBootSafeSeed::GetPermanentConsistencyCountry() const {
  return GetCompressedSeed().permanent_country_code;
}

std::string EarlyBootSafeSeed::GetSessionConsistencyCountry() const {
  return GetCompressedSeed().session_country_code;
}

SeedReaderWriter* EarlyBootSafeSeed::GetSeedReaderWriterForTesting() {
  return nullptr;
}

void EarlyBootSafeSeed::SetSeedReaderWriterForTesting(
    std::unique_ptr<SeedReaderWriter> seed_reader_writer) {}

void EarlyBootSafeSeed::ClearState() {}

}  // namespace variations::cros_early_boot::evaluate_seed
