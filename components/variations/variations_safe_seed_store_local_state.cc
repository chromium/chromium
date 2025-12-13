// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_safe_seed_store_local_state.h"

#include "base/version_info/channel.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/seed_reader_writer.h"

namespace variations {
namespace {

// The name of the seed file that stores the safe seed data and other
// seed-related information in a compressed proto.
const base::FilePath::CharType kSafeSeedFilename[] =
    FILE_PATH_LITERAL("VariationsSafeSeedV2");

// Name of the old safe seed file. It stores only the seed data gzip-compressed.
// TODO(rcanoaparicio): Remove this once the experiment has ended.
const base::FilePath::CharType kOldSafeSeedFilename[] =
    FILE_PATH_LITERAL("VariationsSafeSeedV1");

}  // namespace

VariationsSafeSeedStoreLocalState::VariationsSafeSeedStoreLocalState(
    PrefService* local_state,
    const base::FilePath& seed_file_dir,
    version_info::Channel channel,
    const EntropyProviders* entropy_providers)
    : local_state_(local_state),
      seed_reader_writer_(
          std::make_unique<SeedReaderWriter>(local_state,
                                             seed_file_dir,
                                             kSafeSeedFilename,
                                             kOldSafeSeedFilename,
                                             kSafeSeedFieldsPrefs,
                                             channel,
                                             entropy_providers,
                                             /*histogram_suffix=*/"Safe")) {}

VariationsSafeSeedStoreLocalState::~VariationsSafeSeedStoreLocalState() =
    default;

base::Time VariationsSafeSeedStoreLocalState::GetFetchTime() const {
  return seed_reader_writer_->GetSeedInfo().client_fetch_time;
}

void VariationsSafeSeedStoreLocalState::SetFetchTime(
    const base::Time& fetch_time) {
  seed_reader_writer_->SetFetchTime(fetch_time);
}

int VariationsSafeSeedStoreLocalState::GetMilestone() const {
  return seed_reader_writer_->GetSeedInfo().milestone;
}

base::Time VariationsSafeSeedStoreLocalState::GetTimeForStudyDateChecks()
    const {
  return seed_reader_writer_->GetSeedInfo().seed_date;
}

StoreSeedResult VariationsSafeSeedStoreLocalState::SetCompressedSeed(
    ValidatedSeedInfo seed_info) {
  return seed_reader_writer_->StoreValidatedSeedInfo(seed_info);
}

std::string VariationsSafeSeedStoreLocalState::GetLocale() const {
  return local_state_->GetString(prefs::kVariationsSafeSeedLocale);
}

void VariationsSafeSeedStoreLocalState::SetLocale(const std::string& locale) {
  local_state_->SetString(prefs::kVariationsSafeSeedLocale, locale);
}

std::string VariationsSafeSeedStoreLocalState::GetPermanentConsistencyCountry()
    const {
  return seed_reader_writer_->GetSeedInfo().permanent_country_code;
}

std::string VariationsSafeSeedStoreLocalState::GetSessionConsistencyCountry()
    const {
  return seed_reader_writer_->GetSeedInfo().session_country_code;
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
  // Seed and other related information is cleared by the SeedReaderWriter.
  seed_reader_writer_->ClearSeedInfo();
  seed_reader_writer_->ClearSessionCountry();
  seed_reader_writer_->ClearPermanentConsistencyCountryAndVersion();
  local_state_->ClearPref(prefs::kVariationsSafeSeedLocale);
}

LoadSeedResult VariationsSafeSeedStoreLocalState::ReadSeedData(
    std::string* seed_data,
    std::string* base64_seed_signature) {
  return seed_reader_writer_->ReadSeedDataOnStartup(seed_data,
                                                    base64_seed_signature);
}

void VariationsSafeSeedStoreLocalState::ReadSeedData(
    SeedReaderWriter::ReadSeedDataCallback done_callback) {
  seed_reader_writer_->ReadSeedData(std::move(done_callback));
}

void VariationsSafeSeedStoreLocalState::AllowToPurgeSeedDataFromMemory() {
  seed_reader_writer_->AllowToPurgeSeedDataFromMemory();
}

void VariationsSafeSeedStoreLocalState::GetStoredSeedInfoForDebugging(
    base::OnceCallback<void(StoredSeedInfo)> done_callback) {
  seed_reader_writer_->GetStoredSeedInfoForDebugging(std::move(done_callback));
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
