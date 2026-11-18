// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_safe_seed_store.h"

#include "base/metrics/field_trial.h"
#include "base/version_info/channel.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/pref_names.h"
#include "components/variations/seed_reader_writer.h"

namespace variations {
namespace {

// The name of the seed file that stores the safe seed data and other
// seed-related information in a compressed proto.
const base::FilePath::CharType kSafeSeedFilename[] =
    FILE_PATH_LITERAL("VariationsSafeSeedV2");

// Compare startup and stability metrics among pre-stable Seed File clients,
// independently of the experiment that selects their storage backend. The
// SeedReaderWriter member constructed before the constructor body runs sets up
// kSeedFileTrial without reading the safe file.
bool ShouldDeferSafeSeedRead(const base::FilePath& seed_file_dir,
                             version_info::Channel channel,
                             const EntropyProviders* entropy_providers) {
  if (seed_file_dir.empty() || entropy_providers == nullptr ||
      (channel != version_info::Channel::CANARY &&
       channel != version_info::Channel::DEV &&
       channel != version_info::Channel::BETA) ||
      base::FieldTrialList::FindFullName(kSeedFileTrial) != kSeedFilesGroup) {
    return false;
  }

  base::FieldTrial* trial =
      base::FieldTrialList::Find(kSafeSeedStartupReadTrial);
  if (!trial) {
    // A zero randomization seed uses this trial's name, keeping its assignment
    // independent of the Seed File storage trial's fixed randomization seed.
    trial = base::FieldTrialList::FactoryGetFieldTrial(
        kSafeSeedStartupReadTrial, /*total_probability=*/100, kDefaultGroup,
        entropy_providers->default_entropy(),
        /*randomization_seed=*/0);
    trial->AppendGroup(kSafeSeedStartupReadControlGroup, 50);
    trial->AppendGroup(kSafeSeedStartupReadTreatmentGroup, 50);
  }
  // Activate both groups before their initialization behavior differs, even
  // when the treatment never needs to read the safe file in this session.
  return trial->group_name() == kSafeSeedStartupReadTreatmentGroup;
}

}  // namespace

VariationsSafeSeedStore::VariationsSafeSeedStore(
    PrefService* local_state,
    const base::FilePath& seed_file_dir,
    version_info::Channel channel,
    const EntropyProviders* entropy_providers)
    : local_state_(local_state),
      seed_reader_writer_(std::make_unique<SeedReaderWriter>(
          local_state,
          seed_file_dir,
          kSafeSeedFilename,
          kSafeSeedFieldsPrefs,
          channel,
          entropy_providers,
          /*histogram_suffix=*/"Safe",
          /*file_task_runner=*/nullptr,
          SeedReaderWriter::StartupReadMode::kDeferred)) {
  // Delay the control group's synchronous read until its trial is active.
  if (!ShouldDeferSafeSeedRead(seed_file_dir, channel, entropy_providers)) {
    seed_reader_writer_->EnsureSeedFileReadSync();
  }
}

VariationsSafeSeedStore::~VariationsSafeSeedStore() = default;

bool VariationsSafeSeedStore::IsInitialized() const {
  return seed_reader_writer_->IsInitialized();
}

base::Time VariationsSafeSeedStore::GetFetchTime() const {
  return seed_reader_writer_->GetSeedInfo().client_fetch_time;
}

void VariationsSafeSeedStore::SetFetchTime(const base::Time& fetch_time) {
  seed_reader_writer_->SetFetchTime(fetch_time);
}

int VariationsSafeSeedStore::GetMilestone() const {
  return seed_reader_writer_->GetSeedInfo().milestone;
}

base::Time VariationsSafeSeedStore::GetTimeForStudyDateChecks() const {
  return seed_reader_writer_->GetSeedInfo().seed_date;
}

StoreSeedResult VariationsSafeSeedStore::SetCompressedSeed(
    ValidatedSeedInfo seed_info) {
  return seed_reader_writer_->StoreValidatedSeedInfo(seed_info);
}

std::string VariationsSafeSeedStore::GetLocale() const {
  return local_state_->GetString(prefs::kVariationsSafeSeedLocale);
}

void VariationsSafeSeedStore::SetLocale(const std::string& locale) {
  local_state_->SetString(prefs::kVariationsSafeSeedLocale, locale);
}

std::string VariationsSafeSeedStore::GetPermanentConsistencyCountry() const {
  return seed_reader_writer_->GetSeedInfo().permanent_country_code;
}

std::string VariationsSafeSeedStore::GetSessionConsistencyCountry() const {
  return seed_reader_writer_->GetSeedInfo().session_country_code;
}

SeedReaderWriter* VariationsSafeSeedStore::GetSeedReaderWriterForTesting() {
  return seed_reader_writer_.get();
}

void VariationsSafeSeedStore::SetSeedReaderWriterForTesting(
    std::unique_ptr<SeedReaderWriter> seed_reader_writer) {
  seed_reader_writer_ = std::move(seed_reader_writer);
}

void VariationsSafeSeedStore::ClearState() {
  // Clear the locale before readers can run and update it or destroy the store.
  local_state_->ClearPref(prefs::kVariationsSafeSeedLocale);
  // Seed and other related information is cleared by the SeedReaderWriter.
  seed_reader_writer_->ClearAllSeedInfo();
}

LoadSeedResult VariationsSafeSeedStore::ReadSeedData(
    std::string* seed_data,
    std::string* base64_seed_signature) {
  return seed_reader_writer_->ReadSeedDataOnStartup(seed_data,
                                                    base64_seed_signature);
}

void VariationsSafeSeedStore::ReadSeedData(
    SeedReaderWriter::ReadSeedDataCallback done_callback) {
  seed_reader_writer_->ReadSeedData(std::move(done_callback));
}

void VariationsSafeSeedStore::AllowToPurgeSeedDataFromMemory() {
  seed_reader_writer_->AllowToPurgeSeedDataFromMemory();
}

void VariationsSafeSeedStore::CommitPendingWrite() {
  seed_reader_writer_->CommitPendingWrite();
}

void VariationsSafeSeedStore::GetStoredSeedInfoForDebugging(
    base::OnceCallback<void(StoredSeedInfo)> done_callback) {
  seed_reader_writer_->GetStoredSeedInfoForDebugging(std::move(done_callback));
}

// static
void VariationsSafeSeedStore::RegisterPrefs(PrefRegistrySimple* registry) {
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
