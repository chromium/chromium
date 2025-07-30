// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/seed_reader_writer.h"

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/values_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/version_info/channel.h"
#include "components/prefs/pref_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/pref_names.h"
#include "third_party/zlib/google/compression_utils.h"

namespace variations {
namespace {

// A struct to hold the permanent country code and version. Because they're
// stored in a single pref, we need to read them together.
// TODO(crbug.com/411431524): Remove this once it's stored in the Seed File.
struct PermanentCountryVersion {
  std::string_view country;
  std::string_view version;
};

// Histogram suffix used by ImportantFileWriter for recording seed file write
// information.
constexpr char kSeedWriterHistogramSuffix[] = "VariationsSeedsV1";

// Serializes and returns seed data used during write to disk. Will be run
// asynchronously on a background thread.
std::optional<std::string> DoSerialize(StoredSeedInfo seed_info) {
  // TODO(crbug.com/370480037): Begin doing seed compression here instead of in
  // VariationsSeedStore.
  return seed_info.data();
}

// Returns the file path used to store a seed. If `seed_file_dir` is empty, an
// empty file path is returned.
base::FilePath GetFilePath(const base::FilePath& seed_file_dir,
                           base::FilePath::StringViewType filename) {
  return seed_file_dir.empty() ? base::FilePath()
                               : seed_file_dir.Append(filename);
}

// Returns true if the client is eligible to participate in the seed file trial.
bool IsEligibleForSeedFileTrial(version_info::Channel channel,
                                const base::FilePath& seed_file_dir,
                                const EntropyProviders* entropy_providers) {
  // Note platforms that should not participate in the experiment will
  // deliberately pass an empty |seed_file_dir| and null |entropy_provider|.
  if (seed_file_dir.empty() || entropy_providers == nullptr) {
    return false;
  }
  return channel == version_info::Channel::CANARY ||
         channel == version_info::Channel::DEV ||
         channel == version_info::Channel::BETA ||
         channel == version_info::Channel::STABLE;
}

// Sets up the seed file experiment which only some clients are eligible for
// (see IsEligibleForSeedFileTrial()).
void SetUpSeedFileTrial(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    version_info::Channel channel) {
  // Verify that the field trial has not already been set up. This may be the
  // case if a SeedReaderWriter associated with a safe seed calls this function
  // before one associated with a latest seed or vice versa.
  if (base::FieldTrialList::TrialExists(kSeedFileTrial)) {
    return;
  }

  // Only 1% of clients on stable should participate in the experiment.
  base::FieldTrial::Probability group_probability =
      channel == version_info::Channel::STABLE ? 1 : 50;

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kSeedFileTrial, /*total_probability=*/100, kDefaultGroup,
          entropy_provider));

  trial->AppendGroup(kControlGroup, group_probability);
  trial->AppendGroup(kSeedFilesGroup, group_probability);
}

// Returns the permanent country code and version. For the safe seed, version
// always will be empty.
PermanentCountryVersion GetPermanentCountryVersion(PrefService* local_state,
                                                   std::string_view pref_name) {
  // TODO(crbug.com/411431524): Remove this once it's stored in the Seed File.
  // We need to check because the safe seed pref is a string while the latest
  // seed pref is a list.
  if (pref_name == prefs::kVariationsSafeSeedPermanentConsistencyCountry) {
    return {.country = local_state->GetString(pref_name), .version = ""};
  }
  const auto& list_value = local_state->GetList(pref_name);
  PermanentCountryVersion result;
  if (list_value.size() == 2) {
    const std::string* stored_version = nullptr;
    // We don't need to check the validity of the version here, as it's done
    // later by
    // VariationsFieldTrialCreatorBase::LoadPermanentConsistencyCountry().
    if ((stored_version = list_value[0].GetIfString())) {
      result.version = *stored_version;
    }
    const std::string* stored_country = nullptr;
    if ((stored_country = list_value[1].GetIfString())) {
      result.country = *stored_country;
    }
  }
  return result;
}

// Stores the permanent country code and version in local state. For the safe
// seed, the version is always empty.
void SetPermanentCountryVersion(PrefService* local_state,
                                std::string_view pref_name,
                                std::string_view country_code,
                                std::string_view version) {
  // TODO(crbug.com/411431524): Remove this once it's stored in the Seed File.
  // We need to check because the safe seed pref is a string while the latest
  // seed pref is a list.
  const bool is_safe_seed =
      pref_name == prefs::kVariationsSafeSeedPermanentConsistencyCountry;
  if (is_safe_seed) {
    local_state->SetString(pref_name, country_code);
  } else {
    base::Value::List list_value;
    list_value.Append(version);
    list_value.Append(country_code);
    local_state->SetList(pref_name, std::move(list_value));
  }
}

int64_t TimeToProtoTime(base::Time time) {
  return time.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

base::Time ProtoTimeToTime(int64_t proto_time) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(proto_time));
}

}  // namespace

const SeedFieldsPrefs kRegularSeedFieldsPrefs = {
    .seed = prefs::kVariationsCompressedSeed,
    .signature = prefs::kVariationsSeedSignature,
    .milestone = prefs::kVariationsSeedMilestone,
    .seed_date = prefs::kVariationsSeedDate,
    .client_fetch_time = prefs::kVariationsLastFetchTime,
    .session_country_code = prefs::kVariationsCountry,
    .permanent_country_code_version =
        prefs::kVariationsPermanentConsistencyCountry,
};

const SeedFieldsPrefs kSafeSeedFieldsPrefs = {
    .seed = prefs::kVariationsSafeCompressedSeed,
    .signature = prefs::kVariationsSafeSeedSignature,
    .milestone = prefs::kVariationsSafeSeedMilestone,
    .seed_date = prefs::kVariationsSafeSeedDate,
    .client_fetch_time = prefs::kVariationsSafeSeedFetchTime,
    .session_country_code = prefs::kVariationsSafeSeedSessionConsistencyCountry,
    .permanent_country_code_version =
        prefs::kVariationsSafeSeedPermanentConsistencyCountry,
};

StoredSeed::StoredSeed(StorageFormat storage_format,
                       std::string_view data,
                       std::string_view signature,
                       int milestone,
                       base::Time seed_date,
                       base::Time client_fetch_time,
                       std::string_view session_country_code,
                       std::string_view permanent_country_code,
                       std::string_view permanent_country_version)
    : storage_format(storage_format),
      data(data),
      signature(signature),
      milestone(milestone),
      seed_date(seed_date),
      client_fetch_time(client_fetch_time),
      session_country_code(session_country_code),
      permanent_country_code(permanent_country_code),
      permanent_country_version(permanent_country_version) {}

StoredSeed::~StoredSeed() = default;

SeedReaderWriter::SeedReaderWriter(
    PrefService* local_state,
    const base::FilePath& seed_file_dir,
    base::FilePath::StringViewType seed_filename,
    const SeedFieldsPrefs& fields_prefs,
    version_info::Channel channel,
    const EntropyProviders* entropy_providers,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : local_state_(local_state),
      fields_prefs_(fields_prefs),
      file_task_runner_(std::move(file_task_runner)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(local_state_) << "SeedReaderWriter needs a valid local state.";
  if (!seed_file_dir.empty()) {
    seed_writer_ = std::make_unique<base::ImportantFileWriter>(
        GetFilePath(seed_file_dir, seed_filename), file_task_runner_,
        kSeedWriterHistogramSuffix);
  }
  if (IsEligibleForSeedFileTrial(channel, seed_file_dir, entropy_providers)) {
    SetUpSeedFileTrial(entropy_providers->default_entropy(), channel);
    if (ShouldUseSeedFile()) {
      ReadSeedFile();
    }
  }
}

SeedReaderWriter::~SeedReaderWriter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (HasPendingWrite()) {
    seed_writer_->DoScheduledWrite();
  }
}

void SeedReaderWriter::StoreValidatedSeedInfo(ValidatedSeedInfo seed_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    ScheduleSeedFileWrite(seed_info);
  } else {
    ScheduleLocalStateWrite(seed_info);
  }
}

void SeedReaderWriter::ClearSeedInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/372009105): Remove if-statements when experiment has ended.
  if (ShouldUseSeedFile()) {
    ScheduleSeedFileClear();
  } else {
    local_state_->ClearPref(fields_prefs_->seed);
    local_state_->ClearPref(fields_prefs_->signature);
    local_state_->ClearPref(fields_prefs_->milestone);
    local_state_->ClearPref(fields_prefs_->seed_date);
    local_state_->ClearPref(fields_prefs_->client_fetch_time);
    // Although only clients in the treatment group write seeds to dedicated
    // seed files, attempt to delete the seed file for clients with
    // Local-State-based seeds. If a client switches experiment groups or
    // channels, their device could have a seed file with stale seed data.
    if (seed_writer_) {
      DeleteSeedFile();
    }
  }
}

void SeedReaderWriter::ClearSessionCountry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    seed_info_.clear_session_country_code();
  }
  local_state_->ClearPref(fields_prefs_->session_country_code);
}

StoredSeed SeedReaderWriter::GetSeedData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    return StoredSeed(
        /*storage_format=*/StoredSeed::StorageFormat::kCompressed,
        /*data=*/seed_info_.data(),
        /*signature=*/seed_info_.signature(),
        /*milestone=*/seed_info_.milestone(),
        /*seed_date=*/ProtoTimeToTime(seed_info_.seed_date()),
        /*client_fetch_time=*/ProtoTimeToTime(seed_info_.client_fetch_time()),
        /*session_country_code=*/seed_info_.session_country_code(),
        /*permanent_country_code=*/seed_info_.permanent_country_code(),
        /*permanent_country_version=*/seed_info_.permanent_version());
  } else {
    PermanentCountryVersion permanent_country_version =
        GetPermanentCountryVersion(
            local_state_, fields_prefs_->permanent_country_code_version);
    return StoredSeed(
        /*storage_format=*/StoredSeed::StorageFormat::
            kCompressedAndBase64Encoded,
        /*data=*/local_state_->GetString(fields_prefs_->seed),
        /*signature=*/local_state_->GetString(fields_prefs_->signature),
        /*milestone=*/local_state_->GetInteger(fields_prefs_->milestone),
        /*seed_date=*/local_state_->GetTime(fields_prefs_->seed_date),
        /*client_fetch_time=*/
        local_state_->GetTime(fields_prefs_->client_fetch_time),
        /*session_country_code=*/
        local_state_->GetString(fields_prefs_->session_country_code),
        /*permanent_country_code=*/permanent_country_version.country,
        /*permanent_country_version=*/permanent_country_version.version);
  }
}

void SeedReaderWriter::SetTimerForTesting(base::OneShotTimer* timer_override) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (seed_writer_) {
    seed_writer_->SetTimerForTesting(timer_override);  // IN-TEST
  }
}

void SeedReaderWriter::SetSeedDate(base::Time server_date_fetched) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Both groups write the seed date to local state.
  // TODO(crbug.com/380465790): Update seed date in seed files instead of local
  // state if the client is in the treatment group.
  if (ShouldUseSeedFile()) {
    seed_info_.set_seed_date(TimeToProtoTime(server_date_fetched));
  }
  local_state_->SetTime(fields_prefs_->seed_date, server_date_fetched);
}

void SeedReaderWriter::SetFetchTime(base::Time fetch_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Both groups write the fetch time to local state.
  // TODO(crbug.com/380465790): Update fetch time in seed files instead of local
  // state if the client is in the treatment group.
  if (ShouldUseSeedFile()) {
    seed_info_.set_client_fetch_time(TimeToProtoTime(fetch_time));
  }
  local_state_->SetTime(fields_prefs_->client_fetch_time, fetch_time);
}

bool SeedReaderWriter::HasPendingWrite() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return seed_writer_ && seed_writer_->HasPendingWrite();
}

void SeedReaderWriter::ClearPermanentConsistencyCountryAndVersion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    // TODO(crbug.com/380465790): Clear the values from the seed file if the
    // client is in the treatment group.
    seed_info_.clear_permanent_country_code();
    seed_info_.clear_permanent_version();
  }
  local_state_->ClearPref(fields_prefs_->permanent_country_code_version);
}

void SeedReaderWriter::SetPermanentConsistencyCountryAndVersion(
    const std::string_view country,
    const std::string_view version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    seed_info_.set_permanent_country_code(country);
    seed_info_.set_permanent_version(version);
  }
  SetPermanentCountryVersion(local_state_,
                             fields_prefs_->permanent_country_code_version,
                             country, version);
}

LoadSeedResult SeedReaderWriter::ReadSeedData(
    std::string* seed_data,
    std::string* base64_seed_signature) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const StoredSeed stored_seed = GetSeedData();
  if (stored_seed.data.empty()) {
    return LoadSeedResult::kEmpty;
  }

  // As a space optimization, the latest seed might not be stored directly, but
  // rather aliased to the safe seed. We don't need to store the signature,
  // since it is the same as the safe seed.
  if (stored_seed.data == kIdenticalToSafeSeedSentinel) {
    *seed_data = stored_seed.data;
    return LoadSeedResult::kSuccess;
  }

  std::string_view compressed_data;
  std::string decoded_data;
  switch (stored_seed.storage_format) {
    case StoredSeed::StorageFormat::kCompressed:
      compressed_data = stored_seed.data;
      break;
    // Because clients not using a seed file get seed data from local state
    // instead, they need to decode the base64-encoded seed data first.
    case StoredSeed::StorageFormat::kCompressedAndBase64Encoded:
      if (!base::Base64Decode(stored_seed.data, &decoded_data)) {
        return LoadSeedResult::kCorruptBase64;
      }
      compressed_data = decoded_data;
      break;
  }

  // A corrupt seed could result in a very large buffer being allocated which
  // could crash the process.
  // The maximum size of an uncompressed seed at 50 MiB.
  constexpr std::size_t kMaxUncompressedSeedSize = 50 * 1024 * 1024;
  if (compression::GetUncompressedSize(compressed_data) >
      kMaxUncompressedSeedSize) {
    return LoadSeedResult::kExceedsUncompressedSizeLimit;
  }
  if (!compression::GzipUncompress(compressed_data, seed_data)) {
    return LoadSeedResult::kCorruptGzip;
  }

  // Copy the signature from the loaded seed.
  if (base64_seed_signature) {
    *base64_seed_signature = stored_seed.signature;
  }
  return LoadSeedResult::kSuccess;
}

base::ImportantFileWriter::BackgroundDataProducerCallback
SeedReaderWriter::GetSerializedDataProducerForBackgroundSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // DoSerialize() will be run on a background thread different than the one
  // this function runs on, so `seed_info_.data` is passed as a copy to avoid
  // potential race condition in which the `seed_info_.data is potentially
  // modified at the same time DoSerialize() attempts to access it. We cannot
  // use std::move here as we may attempt to read `seed_info_.data` from memory
  // after a write and before we modify `seed_info_.data` again, in which case
  // unexpected empty data would be read.
  // TODO(crbug.com/370539202) Potentially use std::move instead of copy if we
  // are able to move seed data out of memory.
  return base::BindOnce(&DoSerialize, seed_info_);
}

void SeedReaderWriter::ScheduleSeedFileWrite(ValidatedSeedInfo seed_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set `seed_info_.data`, this will be used later by the background
  // serialization and can be changed multiple times before a scheduled write
  // completes, in which case the background serializer will use the
  // `seed_info_.data` set at the last call of this function.
  seed_info_.set_data(seed_info.compressed_seed_data);
  seed_info_.set_signature(seed_info.signature);
  seed_info_.set_milestone(seed_info.milestone);
  seed_info_.set_seed_date(TimeToProtoTime(seed_info.seed_date));
  seed_info_.set_client_fetch_time(
      TimeToProtoTime(seed_info.client_fetch_time));
  // Only update the latest country code if it is not empty.
  if (!seed_info.session_country_code.empty()) {
    seed_info_.set_session_country_code(seed_info.session_country_code);
  }
  if (!seed_info.permanent_country_code.empty()) {
    seed_info_.set_permanent_country_code(seed_info.permanent_country_code);
  }
  if (!seed_info.permanent_country_version.empty()) {
    seed_info_.set_permanent_version(seed_info.permanent_country_version);
  }
  // `seed_writer_` will eventually call
  // GetSerializedDataProducerForBackgroundSequence() on *this* object to get
  // a callback that will be run asynchronously. This callback will be used to
  // call the DoSerialize() function which will return the seed data to write
  // to the file. This write will also be asynchronous and on a different
  // thread. Note that it is okay to call this while a write is already
  // occurring in a background thread and that this will result in a new write
  // being scheduled.
  seed_writer_->ScheduleWriteWithBackgroundDataSerializer(this);
  // TODO(crbug.com/380465790): Seed-related info that has not yet been migrated
  // to seed files must continue to be maintained in local state. Once the
  // migration is complete, stop updating local state.
  local_state_->SetString(fields_prefs_->signature, seed_info_.signature());
  local_state_->SetInteger(fields_prefs_->milestone, seed_info_.milestone());
  local_state_->SetTime(fields_prefs_->seed_date,
                        ProtoTimeToTime(seed_info_.seed_date()));
  local_state_->SetTime(fields_prefs_->client_fetch_time,
                        ProtoTimeToTime(seed_info_.client_fetch_time()));
  if (!seed_info.session_country_code.empty()) {
    local_state_->SetString(fields_prefs_->session_country_code,
                            seed_info_.session_country_code());
  }
  // Version could be empty in case of the SafeSeed.
  if (!seed_info.permanent_country_code.empty()) {
    SetPermanentCountryVersion(local_state_,
                               fields_prefs_->permanent_country_code_version,
                               seed_info_.permanent_country_code(),
                               seed_info_.permanent_version());
  }
}

void SeedReaderWriter::ScheduleSeedFileClear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set `seed_info_.data`, this will be used later by the background
  // serialization and can be changed multiple times before a scheduled write
  // completes, in which case the background serializer will use the
  // `seed_info_.data` set at the last call of this function.
  seed_info_.clear_data();
  seed_info_.clear_signature();
  seed_info_.clear_milestone();
  seed_info_.clear_seed_date();
  seed_info_.clear_client_fetch_time();
  // `seed_writer_` will eventually call
  // GetSerializedDataProducerForBackgroundSequence() on *this* object to get
  // a callback that will be run asynchronously. This callback will be used to
  // call the DoSerialize() function which will return the seed data to write
  // to the file. This write will also be asynchronous and on a different
  // thread. Note that it is okay to call this while a write is already
  // occurring in a background thread and that this will result in a new write
  // being scheduled.
  seed_writer_->ScheduleWriteWithBackgroundDataSerializer(this);
  // TODO(crbug.com/380465790): Seed-related info that has not yet been migrated
  // to seed files must continue to be maintained in local state. Once the
  // migration is complete, stop updating local state.
  local_state_->ClearPref(fields_prefs_->signature);
  local_state_->ClearPref(fields_prefs_->milestone);
  local_state_->ClearPref(fields_prefs_->seed_date);
  local_state_->ClearPref(fields_prefs_->client_fetch_time);
}

void SeedReaderWriter::DeleteSeedFile() {
  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                seed_writer_->path()));
}

void SeedReaderWriter::ReadSeedFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string histogram_suffix =
      base::Contains(seed_writer_->path().BaseName().MaybeAsASCII(), "Safe")
          ? "Safe"
          : "Latest";
  std::string seed_file_data;
  const bool success =
      base::ReadFileToString(seed_writer_->path(), &seed_file_data);

  if (success) {
    seed_info_.set_data(std::move(seed_file_data));
    // TODO(crbug.com/380465790): Read other SeedInfo fields from the seed file
    // once it's stored there.
    seed_info_.set_signature(local_state_->GetString(fields_prefs_->signature));
    seed_info_.set_milestone(
        local_state_->GetInteger(fields_prefs_->milestone));
    seed_info_.set_seed_date(
        TimeToProtoTime(local_state_->GetTime(fields_prefs_->seed_date)));
    seed_info_.set_client_fetch_time(TimeToProtoTime(
        local_state_->GetTime(fields_prefs_->client_fetch_time)));
    seed_info_.set_session_country_code(
        local_state_->GetString(fields_prefs_->session_country_code));
    PermanentCountryVersion permanent_country_version =
        GetPermanentCountryVersion(
            local_state_, fields_prefs_->permanent_country_code_version);
    seed_info_.set_permanent_country_code(permanent_country_version.country);
    seed_info_.set_permanent_version(permanent_country_version.version);
  } else {
    // Export seed data from Local State to a seed file in the following cases.
    // 1. Seed file does not exist because this is the first run. For Windows,
    // the first run seed may be stored in Local State, see
    // https://crsrc.org/s?q=file:chrome_feature_list_creator.cc+symbol:SetupInitialPrefs.
    // 2. Seed file does not exist because this is the first time a client is
    // in the seed file experiment's treatment group.
    // 3. Seed file exists and read failed.
    std::string decoded_data;
    if (base::Base64Decode(local_state_->GetString(fields_prefs_->seed),
                           &decoded_data)) {
      PermanentCountryVersion permanent_country_version =
          GetPermanentCountryVersion(
              local_state_, fields_prefs_->permanent_country_code_version);
      ScheduleSeedFileWrite(ValidatedSeedInfo{
          .compressed_seed_data = decoded_data,
          .signature = local_state_->GetString(fields_prefs_->signature),
          .milestone = local_state_->GetInteger(fields_prefs_->milestone),
          .seed_date = local_state_->GetTime(fields_prefs_->seed_date),
          .client_fetch_time =
              local_state_->GetTime(fields_prefs_->client_fetch_time),
          .session_country_code =
              local_state_->GetString(fields_prefs_->session_country_code),
          .permanent_country_code = permanent_country_version.country,
          .permanent_country_version = permanent_country_version.version,
      });

      // Record whether empty data is written to the seed file. This can happen
      // in the following cases.
      // 1. It is the first time a client is in the seed file experiment's
      // treatment group. The seed file does not exist and the local state seed
      // is empty.
      // 2. It is not the first time a client is in the treatment group. A
      // seed file exists, but cannot be read, and since local state is no
      // longer maintained and has been cleared in previous runs, the local
      // state seed written is cleared/ empty.
      // 3. It is not the first time a client is in the treatment group. The
      // seed file was deleted.
      base::UmaHistogramBoolean(
          base::StrCat(
              {"Variations.SeedFileWriteEmptySeed.", histogram_suffix}),
          decoded_data.empty());
    }
  }

  base::UmaHistogramBoolean(
      base::StrCat({"Variations.SeedFileRead.", histogram_suffix}), success);

  // Clients using a seed file should clear seed from local state as it will no
  // longer be used.
  local_state_->ClearPref(fields_prefs_->seed);
}

void SeedReaderWriter::ScheduleLocalStateWrite(ValidatedSeedInfo seed_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_state_->SetString(fields_prefs_->seed, seed_info.base64_seed_data);
  local_state_->SetString(fields_prefs_->signature, seed_info.signature);
  local_state_->SetInteger(fields_prefs_->milestone, seed_info.milestone);
  local_state_->SetTime(fields_prefs_->seed_date, seed_info.seed_date);
  local_state_->SetTime(fields_prefs_->client_fetch_time,
                        seed_info.client_fetch_time);
  if (!seed_info.session_country_code.empty()) {
    local_state_->SetString(fields_prefs_->session_country_code,
                            seed_info.session_country_code);
  }
  // Version could be empty in case of the SafeSeed.
  if (!seed_info.permanent_country_code.empty()) {
    SetPermanentCountryVersion(
        local_state_, fields_prefs_->permanent_country_code_version,
        seed_info.permanent_country_code, seed_info.permanent_country_version);
  }
}

bool SeedReaderWriter::ShouldUseSeedFile() const {
  // Use the plain FieldTrialList API here because the trial is registered
  // client-side in VariationsSeedStore SetUpSeedFileTrial().
  return seed_writer_ &&
         base::FieldTrialList::FindFullName(kSeedFileTrial) == kSeedFilesGroup;
}

}  // namespace variations
