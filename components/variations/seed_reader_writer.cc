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
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"

namespace variations {
namespace {

// Histogram suffix used by ImportantFileWriter for recording seed file write
// information.
constexpr char kSeedWriterHistogramSuffix[] = "VariationsSeedsV1";

// Returns true if a seed file should be used.
bool ShouldUseSeedFile() {
  // Use the plain FieldTrialList API here because the trial is registered
  // client-side in VariationsSeedStore SetUpSeedFileTrial().
  return base::FieldTrialList::FindFullName(kSeedFileTrial) == kSeedFilesGroup;
}

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
                           base::FilePath::StringPieceType filename) {
  return seed_file_dir.empty() ? base::FilePath()
                               : seed_file_dir.Append(filename);
}

}  // namespace

SeedReaderWriter::SeedReaderWriter(
    PrefService* local_state,
    const base::FilePath& seed_file_dir,
    base::FilePath::StringPieceType seed_filename,
    std::string_view seed_pref,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : local_state_(local_state),
      seed_pref_(seed_pref),
      file_task_runner_(std::move(file_task_runner)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!seed_file_dir.empty()) {
    seed_writer_ = std::make_unique<base::ImportantFileWriter>(
        GetFilePath(seed_file_dir, seed_filename), file_task_runner_,
        kSeedWriterHistogramSuffix);
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

void SeedReaderWriter::StoreValidatedSeed(
    const std::string& compressed_seed_data,
    const std::string& base64_seed_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_state_->SetString(seed_pref_, base64_seed_data);
  if (ShouldUseSeedFile()) {
    ScheduleSeedFileWrite(compressed_seed_data);
  }
}

void SeedReaderWriter::ClearSeed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_state_->ClearPref(seed_pref_);
  // TODO(crbug.com/372009105): Remove if-statements when experiment has ended.
  if (!seed_writer_) {
    return;
  }

  // Although only clients in the treatment group write seeds to dedicated seed
  // files, attempt to clear the seed file for all groups here. If a client
  // switches experiment groups or channels, their device could have a seed file
  // with stale seed data.
  if (ShouldUseSeedFile()) {
    ScheduleSeedFileWrite(std::string());
  } else if (base::PathExists(seed_writer_->path())) {
    DeleteSeedFile();
  }
}

const std::string& SeedReaderWriter::GetSeedData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    return seed_data_;
  } else {
    return local_state_->GetString(seed_pref_);
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
  // this function runs on, so `seed_data_` is passed as a copy to avoid
  // potential race condition in which the `seed_data_ is potentially modified
  // at the same time DoSerialize() attempts to access it. We cannot use
  // std::move here as we may attempt to read `seed_data_` from memory after a
  // write and before we modify `seed_data_` again, in which case unexpected
  // empty data would be read.
  // TODO(crbug.com/370539202) Potentially use std::move instead of copy if we
  // are able to move seed data out of memory.
  return base::BindOnce(&DoSerialize, seed_data_);
}

void SeedReaderWriter::ScheduleSeedFileWrite(const std::string& seed_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (seed_writer_) {
    // Set `seed_data_`, this will be used later by the background serialization
    // and can be changed multiple times before a scheduled write completes, in
    // which case the background serializer will use the `seed_data_` set at the
    // last call of this function.
    seed_data_ = seed_data;
    // `seed_writer_` will eventually call
    // GetSerializedDataProducerForBackgroundSequence() on *this* object to get
    // a callback that will be run asynchronously. This callback will be used to
    // call the DoSerialize() function which will return the seed data to write
    // to the file. This write will also be asynchronous and on a different
    // thread. Note that it is okay to call this while a write is already
    // occurring in a background thread and that this will result in a new write
    // being scheduled.
    seed_writer_->ScheduleWriteWithBackgroundDataSerializer(this);
  }
}

void SeedReaderWriter::DeleteSeedFile() {
  if (seed_writer_) {
    file_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                  seed_writer_->path()));
  }
}

void SeedReaderWriter::ReadSeedFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string seed_file_data;
  const bool success =
      base::ReadFileToString(seed_writer_->path(), &seed_file_data);

  if (success) {
    seed_data_ = std::move(seed_file_data);
  } else {
    // Export seed data from Local State to a seed file in the following cases.
    // 1. Seed file does not exists because this is the first run for Windows
    // OS. In this case, the first run seed may be stored in Local State, see
    // https://crsrc.org/s?q=file:chrome_feature_list_creator.cc+symbol:SetupInitialPrefs.
    // 2. Seed file does not exists because this is the first time a client is
    // in the seed file experiment's treatment group.
    // 3. Seed file exists and read failed.
    std::string decoded_data;
    if (base::Base64Decode(local_state_->GetString(seed_pref_),
                           &decoded_data)) {
      // Write will only occur if ShouldUseSeedFile() is true.
      ScheduleSeedFileWrite(decoded_data);
    }
  }

  base::UmaHistogramBoolean(
      base::StrCat({"Variations.SeedFileRead.",
                    base::Contains(
                        seed_writer_->path().BaseName().MaybeAsASCII(), "Safe")
                        ? "Safe"
                        : "Latest"}),
      success);
}

}  // namespace variations
