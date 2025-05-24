// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SEED_READER_WRITER_H_
#define COMPONENTS_VARIATIONS_SEED_READER_WRITER_H_

#include <string>
#include <string_view>

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

class EntropyProviders;

// Trial and group names for the seed file experiment.
const char kSeedFileTrial[] = "SeedFileTrial";
const char kDefaultGroup[] = "Default";
const char kControlGroup[] = "Control_V7";
const char kSeedFilesGroup[] = "SeedFiles_V7";

// Represents a seed and its storage format where clients using
// seed-file-based seeds store compressed data and those using
// local-state-based seeds store compressed, base64 encoded data.
// It also stores other seed-related info.
struct StoredSeed {
  enum class StorageFormat { kCompressed, kCompressedAndBase64Encoded };

  StorageFormat storage_format;
  std::string_view data;
  std::string_view signature;
  int milestone = 0;
};

// Groups the data from a seed and other seed-related info that is validated
// and ready to be stored in a seed file or local state. This struct is passed
// by value, so it must be copyable and lightweight.
struct ValidatedSeedInfo {
  std::string_view compressed_seed_data;
  std::string_view base64_seed_data;
  std::string_view signature;
  int milestone = 0;
};

struct SeedFieldsPrefs {
  const char* seed;
  const char* signature;
  const char* milestone;
};

COMPONENT_EXPORT(VARIATIONS)
extern const SeedFieldsPrefs kRegularSeedFieldsPrefs;
COMPONENT_EXPORT(VARIATIONS) extern const SeedFieldsPrefs kSafeSeedFieldsPrefs;

// Handles reading and writing seeds to disk.
class COMPONENT_EXPORT(VARIATIONS) SeedReaderWriter
    : public base::ImportantFileWriter::BackgroundDataSerializer {
 public:
  // `local_state` provides access to the local state prefs. Must not be null.
  // `seed_file_dir` denotes the directory for storing a seed file. Note that
  // Android Webview intentionally uses an empty path as it uses only local
  // state to store seeds.
  // `seed_filename` is the base name of a file in which seed data is stored.
  // `fields_prefs` is a variations pref struct (kRegularSeedFieldsPrefs or
  // kSafeSeedFieldsPrefs) denoting the prefs for the fields for the type of
  // seed being stored.
  // `channel` describes the release channel of the browser.
  // `entropy_providers` is used to provide entropy when setting up the seed
  // file field trial. If null, the client will not participate in the
  // experiment.
  // `file_task_runner` handles IO-related tasks. Must not be null.
  SeedReaderWriter(PrefService* local_state,
                   const base::FilePath& seed_file_dir,
                   base::FilePath::StringViewType seed_filename,
                   const SeedFieldsPrefs& fields_prefs,
                   version_info::Channel channel,
                   const EntropyProviders* entropy_providers,
                   scoped_refptr<base::SequencedTaskRunner> file_task_runner =
                       base::ThreadPool::CreateSequencedTaskRunner(
                           {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                            base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));

  SeedReaderWriter(const SeedReaderWriter&) = delete;
  SeedReaderWriter& operator=(const SeedReaderWriter&) = delete;

  ~SeedReaderWriter() override;

  // Schedules a write of `compressed_seed_data` to a seed file for some
  // clients (see ShouldUseSeedFile()) and schedules a write of
  // `base64_seed_data` to local state for all other clients. Also stores other
  // seed-related info.
  void StoreValidatedSeedInfo(ValidatedSeedInfo seed_info);

  // Clears seed data and other seed-related info by overwriting it with an
  // empty string.
  // The following fields are cleared: seed data and signature.
  void ClearSeedInfo();

  // Returns stored seed data.
  StoredSeed GetSeedData() const;

  // Overrides the timer used for scheduling writes with `timer_override`.
  void SetTimerForTesting(base::OneShotTimer* timer_override);

  // Returns true if a write is scheduled but has not yet completed.
  bool HasPendingWrite() const;

 private:
  // TODO(crbug.com/380465790): Represents the seed and other related info.
  // This info will be stored together in the SeedFile. Once all the
  // seed-related info is stored in the struct, change it to a proto and use it
  // to serialize and deserialize the data.
  struct SeedInfo {
    std::string data;
    std::string signature;
    int milestone = 0;
  };

  // Returns the serialized data to be written to disk. This is done
  // asynchronously during the write process.
  base::ImportantFileWriter::BackgroundDataProducerCallback
  GetSerializedDataProducerForBackgroundSequence() override;

  // Schedules `seed_info` to be written using `seed_writer_`. Fields with
  // zero/empty values will be ignored. If you want to clear the seed file, use
  // ScheduleSeedFileClear() instead.
  void ScheduleSeedFileWrite(ValidatedSeedInfo seed_info);

  // Schedules `seed_info` to be cleared using `seed_writer_`.
  void ScheduleSeedFileClear();

  // Schedules the deletion of a seed file.
  void DeleteSeedFile();

  // Reads seed data from a seed file, and if the read is successful,
  // populates `seed_info_`. May also schedule a seed file write for some
  // clients on the first run and for clients that are in the seed file
  // experiment's treatment group for the first time. If `seed_pref_` is present
  // in `local state_`, additionally clears it.
  void ReadSeedFile();

  // Schedules a write of `base64_seed_data` to `local_state_`. Fields with
  // zero/empty values will be ignored. If you want to clear the seed file, use
  // ScheduleSeedFileClear() instead.
  void ScheduleLocalStateWrite(ValidatedSeedInfo seed_info);

  // Returns true if a seed file should be used.
  bool ShouldUseSeedFile() const;

  // Pref service used to persist seeds and seed-related info.
  raw_ptr<PrefService> local_state_;

  // Prefs used to store the seed and related info in local state.
  // TODO(crbug.com/380465790): Remove once the info is stored in the SeedFile.
  const raw_ref<const SeedFieldsPrefs> fields_prefs_;

  // Task runner for IO-related operations.
  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // Helper for safely writing a seed. Null if a seed file path is not given.
  std::unique_ptr<base::ImportantFileWriter> seed_writer_;

  // Stored seed info. Used to store a seed applied during field trial
  // setup or a seed fetched from a variations server. Also stores other
  // seed-related info.
  SeedInfo seed_info_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SEED_READER_WRITER_H_
