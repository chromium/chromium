// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_safe_seed_store_local_state.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"

namespace variations {
namespace {

// The name of the seed file that stores the safe seed data.
const base::FilePath::CharType kSafeSeedFilename[] =
    FILE_PATH_LITERAL("VariationsSafeSeedV1");

}  // namespace

VariationsSafeSeedStoreLocalState::VariationsSafeSeedStoreLocalState(
    PrefService* local_state,
    const base::FilePath& seed_file_dir)
    : local_state_(local_state),
      seed_reader_writer_(std::make_unique<SeedReaderWriter>(
          local_state,
          seed_file_dir,
          kSafeSeedFilename,
          prefs::kVariationsSafeCompressedSeed)) {}

VariationsSafeSeedStoreLocalState::~VariationsSafeSeedStoreLocalState() =
    default;

base::Time VariationsSafeSeedStoreLocalState::GetFetchTime() const {
  return local_state_->GetTime(prefs::kVariationsSafeSeedFetchTime);
}

void VariationsSafeSeedStoreLocalState::SetFetchTime(
    const base::Time& fetch_time) {
  local_state_->SetTime(prefs::kVariationsSafeSeedFetchTime, fetch_time);
}

int VariationsSafeSeedStoreLocalState::GetMilestone() const {
  return local_state_->GetInteger(prefs::kVariationsSafeSeedMilestone);
}

void VariationsSafeSeedStoreLocalState::SetMilestone(int milestone) {
  local_state_->SetInteger(prefs::kVariationsSafeSeedMilestone, milestone);
}

base::Time VariationsSafeSeedStoreLocalState::GetTimeForStudyDateChecks()
    const {
  return local_state_->GetTime(prefs::kVariationsSafeSeedDate);
}

void VariationsSafeSeedStoreLocalState::SetTimeForStudyDateChecks(
    const base::Time& safe_seed_time) {
  local_state_->SetTime(prefs::kVariationsSafeSeedDate, safe_seed_time);
}

const std::string& VariationsSafeSeedStoreLocalState::GetCompressedSeed()
    const {
  // TODO(crbug.com/374947675): Use |seed_reader_writer_| to read a seed.
  return local_state_->GetString(prefs::kVariationsSafeCompressedSeed);
}

void VariationsSafeSeedStoreLocalState::SetCompressedSeed(
    const std::string& safe_compressed,
    const std::string& base64_safe_compressed) {
  seed_reader_writer_->StoreValidatedSeed(safe_compressed,
                                          base64_safe_compressed);
}

std::string VariationsSafeSeedStoreLocalState::GetSignature() const {
  return local_state_->GetString(prefs::kVariationsSafeSeedSignature);
}

void VariationsSafeSeedStoreLocalState::SetSignature(
    const std::string& safe_seed_signature) {
  local_state_->SetString(prefs::kVariationsSafeSeedSignature,
                          safe_seed_signature);
}

std::string VariationsSafeSeedStoreLocalState::GetLocale() const {
  return local_state_->GetString(prefs::kVariationsSafeSeedLocale);
}

void VariationsSafeSeedStoreLocalState::SetLocale(const std::string& locale) {
  local_state_->SetString(prefs::kVariationsSafeSeedLocale, locale);
}

std::string VariationsSafeSeedStoreLocalState::GetPermanentConsistencyCountry()
    const {
  return local_state_->GetString(
      prefs::kVariationsSafeSeedPermanentConsistencyCountry);
}

void VariationsSafeSeedStoreLocalState::SetPermanentConsistencyCountry(
    const std::string& permanent_consistency_country) {
  local_state_->SetString(prefs::kVariationsSafeSeedPermanentConsistencyCountry,
                          permanent_consistency_country);
}

std::string VariationsSafeSeedStoreLocalState::GetSessionConsistencyCountry()
    const {
  return local_state_->GetString(
      prefs::kVariationsSafeSeedSessionConsistencyCountry);
}

void VariationsSafeSeedStoreLocalState::SetSessionConsistencyCountry(
    const std::string& session_consistency_country) {
  local_state_->SetString(prefs::kVariationsSafeSeedSessionConsistencyCountry,
                          session_consistency_country);
}

SeedReaderWriter*
VariationsSafeSeedStoreLocalState::GetSeedReaderWriterForTesting() {
  return seed_reader_writer_.get();
}

void VariationsSafeSeedStoreLocalState::SetSeedReaderWriterForTesting(
    std::unique_ptr<SeedReaderWriter> seed_reader_writer) {
  seed_reader_writer_ = std::move(seed_reader_writer);
}

void VariationsSafeSeedStoreLocalState::ClearState() {
  seed_reader_writer_->ClearSeed();
  local_state_->ClearPref(prefs::kVariationsSafeSeedDate);
  local_state_->ClearPref(prefs::kVariationsSafeSeedFetchTime);
  local_state_->ClearPref(prefs::kVariationsSafeSeedLocale);
  local_state_->ClearPref(prefs::kVariationsSafeSeedMilestone);
  local_state_->ClearPref(
      prefs::kVariationsSafeSeedPermanentConsistencyCountry);
  local_state_->ClearPref(prefs::kVariationsSafeSeedSessionConsistencyCountry);
  local_state_->ClearPref(prefs::kVariationsSafeSeedSignature);
}

// static
void VariationsSafeSeedStoreLocalState::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kVariationsSafeCompressedSeed,
                               std::string());
  registry->RegisterTimePref(prefs::kVariationsSafeSeedDate, base::Time());
  registry->RegisterTimePref(prefs::kVariationsSafeSeedFetchTime, base::Time());
  registry->RegisterStringPref(prefs::kVariationsSafeSeedLocale, std::string());
  registry->RegisterIntegerPref(prefs::kVariationsSafeSeedMilestone, 0);
  registry->RegisterStringPref(
      prefs::kVariationsSafeSeedPermanentConsistencyCountry, std::string());
  registry->RegisterStringPref(
      prefs::kVariationsSafeSeedSessionConsistencyCountry, std::string());
  registry->RegisterStringPref(prefs::kVariationsSafeSeedSignature,
                               std::string());
}
}  // namespace variations
