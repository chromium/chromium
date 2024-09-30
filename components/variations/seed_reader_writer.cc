// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/seed_reader_writer.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"

namespace variations {
namespace {

// Histogram suffix used by ImportantFileWriter for recording seed file write
// information.
// TODO(crbug.com/369080917): Add this back when we begin writing.
// constexpr char kSeedWriterHistogramSuffix[] = "VariationsSeedsV1";

// Returns true if a seed should be written to its new storage.
bool ShouldWriteToNewSeedStorage(version_info::Channel channel) {
  return channel == version_info::Channel::CANARY ||
         channel == version_info::Channel::DEV ||
         channel == version_info::Channel::BETA;
}

// Serializes and returns seed data used during write to disk. Will be run
// asynchronously on a background thread.
std::optional<std::string> DoSerialize(std::string seed_data) {
  // TODO(crbug.com/370480037): Begin doing seed compression here instead of in
  // VariationsSeedStore.
  return seed_data;
}

}  // namespace

SeedReaderWriter::SeedReaderWriter(
    PrefService* local_state,
    const base::FilePath& seed_file_path,
    const version_info::Channel channel,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : local_state_(local_state),
      channel_(channel),
      file_task_runner_(std::move(file_task_runner)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!seed_file_path.empty()) {
    seed_writer_ = std::make_unique<base::ImportantFileWriter>(
        seed_file_path, file_task_runner_, std::string());
  }
}

SeedReaderWriter::~SeedReaderWriter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SeedReaderWriter::StoreValidatedSeed(const std::string& seed_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_state_->SetString(prefs::kVariationsCompressedSeed, seed_data);
  MaybeScheduleSeedFileWrite(seed_data);
}

void SeedReaderWriter::ClearSeed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_state_->ClearPref(prefs::kVariationsCompressedSeed);
  MaybeScheduleSeedFileWrite(std::string());
}

void SeedReaderWriter::SetTimerForTesting(base::OneShotTimer* timer_override) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (seed_writer_) {
    seed_writer_->SetTimerForTesting(timer_override);  // IN-TEST
  }
}

bool SeedReaderWriter::HasPendingWriteForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return seed_writer_ ? seed_writer_->HasPendingWrite() : false;
}

// static
base::FilePath SeedReaderWriter::GetFilePath(
    const base::FilePath& user_data_dir,
    const base::FilePath::CharType* filename) {
  return user_data_dir.empty() ? base::FilePath()
                               : user_data_dir.Append(filename);
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

void SeedReaderWriter::MaybeScheduleSeedFileWrite(
    const std::string& seed_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (seed_writer_ && ShouldWriteToNewSeedStorage(channel_)) {
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

}  // namespace variations
