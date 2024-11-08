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

class PrefService;

namespace variations {

// Trial and group names for the seed file experiment.
const char kSeedFileTrial[] = "SeedFileTrial";
const char kDefaultGroup[] = "Default";
const char kControlGroup[] = "Control_V3";
const char kSeedFilesGroup[] = "SeedFiles_V3";

// Handles reading and writing seeds to disk.
class COMPONENT_EXPORT(VARIATIONS) SeedReaderWriter
    : public base::ImportantFileWriter::BackgroundDataSerializer {
 public:
  // `local_state` provides access to the local state prefs. Must not be null.
  // `seed_file_dir` denotes the directory for storing a seed file. Note that
  // Android Webview intentionally uses an empty path as it uses only local
  // state to store seeds.
  // `seed_filename` is the base name of a file in which seed data is stored.
  // `seed_pref` is a variations pref (kVariationsCompressedSeed or
  // kVariationsSafeCompressed) denoting the type of seed handled by this
  // SeedReaderWriter.
  // `file_task_runner` handles IO-related tasks. Must not be null.
  SeedReaderWriter(PrefService* local_state,
                   const base::FilePath& seed_file_dir,
                   base::FilePath::StringPieceType seed_filename,
                   std::string_view seed_pref,
                   scoped_refptr<base::SequencedTaskRunner> file_task_runner =
                       base::ThreadPool::CreateSequencedTaskRunner(
                           {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                            base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));

  SeedReaderWriter(const SeedReaderWriter&) = delete;
  SeedReaderWriter& operator=(const SeedReaderWriter&) = delete;

  ~SeedReaderWriter() override;

  // Schedules a write of `base64_seed_data` to local state. For some clients
  // (see ShouldUseSeedFile()), also schedules a write of `compressed_seed_data`
  // to a seed file.
  void StoreValidatedSeed(const std::string& compressed_seed_data,
                          const std::string& base64_seed_data);

  // Clears seed data by overwriting it with an empty string.
  void ClearSeed();

  // Returns stored seed data.
  const std::string& GetSeedData() const;

  // Overrides the timer used for scheduling writes with `timer_override`.
  void SetTimerForTesting(base::OneShotTimer* timer_override);

 private:
  // Returns the serialized data to be written to disk. This is done
  // asynchronously during the write process.
  base::ImportantFileWriter::BackgroundDataProducerCallback
  GetSerializedDataProducerForBackgroundSequence() override;

  // Schedules `seed_data` to be written using `seed_writer_`.
  void ScheduleSeedFileWrite(const std::string& seed_data);

  // Schedules the deletion of a seed file.
  void DeleteSeedFile();

  // Reads seed data from a seed file, and if the read is successful,
  // populates `seed_data_`. May also schedule a seed file write for
  // some clients on the first run and for clients that are in the seed
  // file experiment's treatment group for the first time.
  void ReadSeedFile();

  // Pref service used to persist seeds.
  raw_ptr<PrefService> local_state_;

  // A variations pref (kVariationsCompressedSeed or kVariationsSafeCompressed)
  // denoting the type of seed handled by this SeedReaderWriter.
  std::string_view seed_pref_;

  // Task runner for IO-related operations.
  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Helper for safely writing a seed. Null if a seed file path is not given.
  std::unique_ptr<base::ImportantFileWriter> seed_writer_;

  // The compressed seed data. Used to store a seed applied during field trial
  // setup or a seed fetched from a variations server.
  std::string seed_data_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SEED_READER_WRITER_H_
