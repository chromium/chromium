// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SEED_READER_WRITER_H_
#define COMPONENTS_VARIATIONS_SEED_READER_WRITER_H_

#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version_info/channel.h"

class PrefService;

namespace variations {

// Handles reading and writing seeds to disk.
class COMPONENT_EXPORT(VARIATIONS) SeedReaderWriter
    : public base::ImportantFileWriter::BackgroundDataSerializer {
 public:
  // `local_state` provides access to the local state prefs. Must not be null.
  // `seed_file_path` denotes a file containing a seed. Note that Android
  // Webview intentionally uses an empty path as it will continue using local
  // state to store seeds.
  // `channel` describes the browser's release channel.
  // `file_task_runner` handles IO-related tasks. Must not be null.
  SeedReaderWriter(PrefService* local_state,
                   const base::FilePath& seed_file_path,
                   const version_info::Channel channel,
                   scoped_refptr<base::SequencedTaskRunner> file_task_runner =
                       base::ThreadPool::CreateSequencedTaskRunner(
                           {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                            base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));

  SeedReaderWriter(const SeedReaderWriter&) = delete;
  SeedReaderWriter& operator=(const SeedReaderWriter&) = delete;

  ~SeedReaderWriter() override;

  // Schedules a write of `serialized_data` to disk. For some clients (see
  // ShouldWriteToNewSeedStorage()), two writes are scheduled.
  void StoreValidatedSeed(const std::string& serialized_data);

  // Clears seed data by overriding it with an empty string.
  void ClearSeed();

  // Overrides the timer used for scheduling writes with `timer_override`.
  void SetTimerForTesting(base::OneShotTimer* timer_override);

  // Returns whether or not `seed_writer_` currently has a write
  // scheduled/pending.
  bool HasPendingWriteForTesting() const;

  // Returns the file path used to store a seed. If `user_data_dir` is empty, an
  // empty file path is returned.
  static base::FilePath GetFilePath(const base::FilePath& user_data_dir,
                                    const base::FilePath::CharType* filename);

 private:
  // Returns the serialized data to be written to disk. This is done
  // asynchronously during the write process.
  base::ImportantFileWriter::BackgroundDataProducerCallback
  GetSerializedDataProducerForBackgroundSequence() override;

  // Schedules `seed_data` to be written using `seed_writer_`.
  void MaybeScheduleSeedFileWrite(const std::string& seed_data);

  // Pref service used to persist seeds.
  raw_ptr<PrefService> local_state_;

  // Channel the client is apart of.
  const version_info::Channel channel_;

  // Task runner for IO-related operations.
  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Helper for safely writing a seed. Null if a seed file path is not given.
  std::unique_ptr<base::ImportantFileWriter> seed_writer_;

  // The compressed seed data.
  std::string seed_data_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SEED_READER_WRITER_H_
