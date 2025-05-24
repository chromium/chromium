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
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/version_info/channel.h"
#include "components/prefs/pref_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/pref_names.h"

namespace variations {
namespace {

// Histogram suffix used by ImportantFileWriter for recording seed file write
// information.
constexpr char kSeedWriterHistogramSuffix[] = "VariationsSeedsV1";

// Serializes and returns seed data used during write to disk. Will be run
// asynchronously on a background thread.
std::optional<std::string> DoSerialize(std::string seed_data) {
  // TODO(crbug.com/370480037): Begin doing seed compression here instead of in
  // VariationsSeedStore.
  return seed_data;
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

}  // namespace

const SeedFieldsPrefs kRegularSeedFieldsPrefs = {
    .seed = prefs::kVariationsCompressedSeed,
    .signature = prefs::kVariationsSeedSignature,
    .milestone = prefs::kVariationsSeedMilestone};

const SeedFieldsPrefs kSafeSeedFieldsPrefs = {
    .seed = prefs::kVariationsSafeCompressedSeed,
    .signature = prefs::kVariationsSafeSeedSignature,
    .milestone = prefs::kVariationsSafeSeedMilestone};

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
  if (seed_writer_ && seed_writer_->HasPendingWrite()) {
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
    // Although only clients in the treatment group write seeds to dedicated
    // seed files, attempt to delete the seed file for clients with
    // Local-State-based seeds. If a client switches experiment groups or
    // channels, their device could have a seed file with stale seed data.
    if (seed_writer_) {
      DeleteSeedFile();
    }
  }
}
StoredSeed SeedReaderWriter::GetSeedData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    return StoredSeed{.storage_format = StoredSeed::StorageFormat::kCompressed,
                      .data = seed_info_.data,
                      .signature = seed_info_.signature,
                      .milestone = seed_info_.milestone};
  } else {
    return StoredSeed{
        .storage_format =
            StoredSeed::StorageFormat::kCompressedAndBase64Encoded,
        .data = local_state_->GetString(fields_prefs_->seed),
        .signature = local_state_->GetString(fields_prefs_->signature),
        .milestone = local_state_->GetInteger(fields_prefs_->milestone)};
  }
}

void SeedReaderWriter::SetTimerForTesting(base::OneShotTimer* timer_override) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (seed_writer_) {
    seed_writer_->SetTimerForTesting(timer_override);  // IN-TEST
  }
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
  return base::BindOnce(&DoSerialize, seed_info_.data);
}

void SeedReaderWriter::ScheduleSeedFileWrite(ValidatedSeedInfo seed_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set `seed_info_.data`, this will be used later by the background
  // serialization and can be changed multiple times before a scheduled write
  // completes, in which case the background serializer will use the
  // `seed_info_.data` set at the last call of this function.
  seed_info_.data = seed_info.compressed_seed_data;
  seed_info_.signature = seed_info.signature;
  seed_info_.milestone = seed_info.milestone;
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
  local_state_->SetString(fields_prefs_->signature, seed_info_.signature);
  local_state_->SetInteger(fields_prefs_->milestone, seed_info_.milestone);
}

void SeedReaderWriter::ScheduleSeedFileClear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set `seed_info_.data`, this will be used later by the background
  // serialization and can be changed multiple times before a scheduled write
  // completes, in which case the background serializer will use the
  // `seed_info_.data` set at the last call of this function.
  seed_info_ = {
      .data = "",
      .signature = "",
      .milestone = 0,
  };
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
    seed_info_.data = std::move(seed_file_data);
    // TODO(crbug.com/380465790): Read other SeedInfo fields from the seed file
    // once it's stored there.
    seed_info_.signature = local_state_->GetString(fields_prefs_->signature);
    seed_info_.milestone = local_state_->GetInteger(fields_prefs_->milestone);
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
      ScheduleSeedFileWrite(ValidatedSeedInfo{
          .compressed_seed_data = decoded_data,
          .signature = local_state_->GetString(fields_prefs_->signature),
          .milestone = local_state_->GetInteger(fields_prefs_->milestone),
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
}

bool SeedReaderWriter::ShouldUseSeedFile() const {
  // Use the plain FieldTrialList API here because the trial is registered
  // client-side in VariationsSeedStore SetUpSeedFileTrial().
  return seed_writer_ &&
         base::FieldTrialList::FindFullName(kSeedFileTrial) == kSeedFilesGroup;
}

bool SeedReaderWriter::HasPendingWrite() const {
  return seed_writer_ && seed_writer_->HasPendingWrite();
}

}  // namespace variations
