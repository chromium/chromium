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
#include "components/variations/metrics.h"
#include "components/variations/proto/stored_seed_info.pb.h"

class PrefService;

namespace variations {

class EntropyProviders;

// Trial and group names for the seed file experiment.
const char kSeedFileTrial[] = "SeedFileTrial";
const char kDefaultGroup[] = "Default";
const char kControlGroup[] = "Control_V7";
const char kSeedFilesGroup[] = "SeedFiles_V7";

// A sentinel value that may be stored as the latest variations seed value in
// to indicate that the latest seed is identical to the safe seed. Used to avoid
// duplicating storage space.
inline constexpr char kIdenticalToSafeSeedSentinel[] = "safe_seed_content";

// Represents seed-related info that, contrary to the seed data, is stored in
// memory, so it can be read synchronously.
struct COMPONENT_EXPORT(VARIATIONS) SeedInfo {
  SeedInfo(std::string_view signature,
           int milestone,
           base::Time seed_date,
           base::Time client_fetch_time,
           std::string_view session_country_code,
           std::string_view permanent_country_code,
           std::string_view permanent_country_version);
  ~SeedInfo();
  SeedInfo(const SeedInfo& other);

  // base64-encoded signature of the seed.
  const std::string_view signature;
  // The milestone with which the seed was fetched
  const int milestone = 0;
  // Date used for study date checks. Is a server-provided timestamp.
  // On some platforms, on the first run, it's set to a client-provided
  // timestamp until the server-provided timestamp is fetched. (See
  // ChromeFeatureListCreator::SetupInitialPrefs())
  const base::Time seed_date;
  // The time at which the seed was fetched. This is always a client-side
  // timestamp.
  const base::Time client_fetch_time;
  // Latest country code fetched from the server. Used for evaluating session
  // consistency studies.
  const std::string session_country_code;
  // Country code used for evaluating permanent consistency studies.
  const std::string permanent_country_code;
  // Chrome version at the time `permanent_country_code` was updated.
  const std::string permanent_country_version;
};

// Groups the data from a seed and other seed-related info that is validated
// and ready to be stored in a seed file or local state. This struct is passed
// by value, so it must be copyable and lightweight.
struct ValidatedSeedInfo {
  const std::string_view seed_data;
  const std::string_view signature;
  const int milestone = 0;
  const base::Time seed_date;
  const base::Time client_fetch_time;
  const std::string_view session_country_code;
  const std::string_view permanent_country_code;
  const std::string_view permanent_country_version;
};

struct SeedFieldsPrefs {
  const char* seed;
  const char* signature;
  const char* milestone;
  const char* seed_date;
  const char* client_fetch_time;
  const char* session_country_code;
  const char* permanent_country_code_version;
};

COMPONENT_EXPORT(VARIATIONS)
extern const SeedFieldsPrefs kRegularSeedFieldsPrefs;
COMPONENT_EXPORT(VARIATIONS) extern const SeedFieldsPrefs kSafeSeedFieldsPrefs;

// Handles reading and writing seeds to disk.
class COMPONENT_EXPORT(VARIATIONS) SeedReaderWriter
    : public base::ImportantFileWriter::BackgroundDataSerializer {
 public:
  // Result of a seed read, the seed data, and the signature. The
  // seed data and signature should only be used if the result is
  // `LoadSeedResult::kSuccess`.
  struct ReadSeedDataResult {
    LoadSeedResult result;
    std::string seed_data;
    std::string signature;
  };

  using ReadSeedDataCallback = base::OnceCallback<void(ReadSeedDataResult)>;

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

  // Schedules a write of the compressed seed data to a seed file for some
  // clients (see ShouldUseSeedFile()) and schedules a write of the compressed
  // and base64-encoded seed data to local state for all other clients. Also
  // stores other seed-related info in local state.  `permanent_country_version`
  // should be empty for the safe seed.
  StoreSeedResult StoreValidatedSeedInfo(ValidatedSeedInfo seed_info);

  // Clears seed data and other seed-related info. The following fields are
  // cleared: seed data, signature, milestone, seed_date and client_fetch_time.
  // To clear the session_country_code, use ClearSessionCountry() instead.
  // To clear permanent_country_code and version, use
  // ClearPermanentConsistencyCountryAndVersion() instead.
  void ClearSeedInfo();

  // Clears the session country code.
  void ClearSessionCountry();

  // Returns stored seed-related info. Contrary to the seed data, the data is
  // stored in memory, so it can be read synchronously.
  SeedInfo GetSeedInfo() const;

  // Overrides the timer used for scheduling writes with `timer_override`.
  void SetTimerForTesting(base::OneShotTimer* timer_override);

  // Updates the server-provided seed date that is used for study date checks.
  void SetSeedDate(base::Time server_date_fetched);

  // Updates the time of the last fetch of the seed.
  void SetFetchTime(base::Time client_fetch_time);

  // Returns true if a write is scheduled but has not yet completed.
  bool HasPendingWrite() const;

  // Clears the permanent consistency country and version.
  void ClearPermanentConsistencyCountryAndVersion();

  // Sets the permanent consistency country and version.
  void SetPermanentConsistencyCountryAndVersion(std::string_view country,
                                                std::string_view version);

  // Reads seed data and returns the result of the load. If a pointer for the
  // signature is provided, the signature will be read and stored into
  // `base64_seed_signature`. The value stored into `seed_data` should only be
  // used if the result is `LoadSeedResult::kSuccess`. This version of the API
  // is synchronous and may not be called after
  // AllowToPurgeSeedDataFromMemory(), since it may block the UI thread. Use the
  // callback version ReadSeedData() below instead.
  LoadSeedResult ReadSeedDataOnStartup(
      std::string* seed_data,
      std::string* base64_seed_signature = nullptr);

  // Reads and processes seed data and calls `done_callback` with the result of
  // the load, the seed data, and the signature. The seed data and signature
  // should only be used if the result is `LoadSeedResult::kSuccess`.
  void ReadSeedData(ReadSeedDataCallback done_callback);

  // Stores the seed without applying any extra processing or validation. This
  // is used to store invalid data for testing.
  void StoreRawSeedForTesting(std::string seed_data);

  // Stores a base64-encoded gzipped seed and base64-encoded signature. This is
  // the format used when passing the seed by argument in tests.
  void StoreBase64EncodedSeedAndSignatureForTesting(
      std::string base64_compressed_data,
      std::string base64_signature);

  // Returns true if the seed data is the same as the safe seed sentinel.
  bool IsIdenticalToSafeSeedSentinel();

  // After this is called, the seed data will not be kept in memory after
  // being written to disk, unless it's empty or it's the sentinel value.
  void AllowToPurgeSeedDataFromMemory();

  std::optional<std::string> stored_seed_data_for_testing() const {
    return stored_seed_data_;
  }

 private:
  // The storage format of the seed. Seed-file-based seeds are compressed while
  // local-state-based seeds are compressed and base64 encoded.
  enum class SeedStorageFormat { kCompressed, kCompressedAndBase64Encoded };

  // Callback for GetSeedData(). The arguments are the storage format, the seed
  // data, and the signature.
  using GetSeedDataCallback =
      base::OnceCallback<void(SeedStorageFormat, std::string, std::string)>;

  // Returns the serialized data to be written to disk. This is done
  // asynchronously during the write process.
  base::ImportantFileWriter::BackgroundDataProducerCallback
  GetSerializedDataProducerForBackgroundSequence() override;

  // Returns true if the seed data should be cleared from memory.
  // This is true if:
  // - `seed_purgeable_from_memory_` is false.
  // - No write is pending.
  // - The stored seed data is not empty and not the sentinel value.
  bool ShouldClearSeedDataFromMemory();

  // Called when a seed write is complete. If `seed_purgeable_from_memory_` is
  // false, the stored seed data will be cleared from memory.
  void OnSeedWriteComplete(bool write_success);

  // Schedules `seed_info` to be written using `seed_writer_`. If a field is
  // empty, it will not be updated. If you want to clear the seed file, use
  // ScheduleSeedFileClear() instead.
  StoreSeedResult ScheduleSeedFileWrite(ValidatedSeedInfo seed_info);

  // Schedules `seed_info_` to be cleared using `seed_writer_`. See
  // VariationsSeedStore::ClearPrefs() .
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
  StoreSeedResult ScheduleLocalStateWrite(ValidatedSeedInfo seed_info);

  // Returns true if a seed file should be used.
  bool ShouldUseSeedFile() const;

  // Returns true if the client should migrate to local state from the seed
  // file.
  bool ShouldMigrateToLocalState(version_info::Channel channel) const;

  // Reads the seed data from the seed file and stores it in local state. Also
  // removes the seed file.
  void MigrateToLocalState();

  // Calls `done_callback` with the result of the load, the seed data, and
  // signature. The seed data and signature should only be used if the result is
  // `LoadSeedResult::kSuccess`.
  void ProcessStoredSeedDataAndRunCallback(ReadSeedDataCallback done_callback,
                                           SeedStorageFormat storage_format,
                                           std::string seed_data,
                                           std::string signature);

  // Calls `done_callback` with the result of the load. If the seed file needs
  // to be read, the read will be done in a background thread. The seed data
  // won't be processed, if the seed needs to be used, use ReadSeedData()
  // instead.
  void GetSeedData(GetSeedDataCallback done_callback);

  // Processes the stored seed data and returns the result of the load. If a
  // pointer for the `signature` is provided, the signature will be read and
  // stored into it. The value stored into `seed_data` and `signature`
  // should only be used if the result is `LoadSeedResult::kSuccess`.
  static LoadSeedResult ProcessStoredSeedData(
      SeedStorageFormat storage_format,
      std::string_view stored_seed_data,
      std::string_view stored_seed_signature,
      std::string* seed_data,
      std::string* signature = nullptr);

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
  StoredSeedInfo stored_seed_info_;

  // Seed data stored in memory. It will be set to std::nullopt if the seed data
  // is not stored in memory. In contrast, empty string means the seed data is
  // empty, so it doesn't need to be read from disk.
  std::optional<std::string> stored_seed_data_;

  // Whether to keep the seed data in memory. This is used to avoid storing the
  // seed data in memory when it is not needed. It will be set to true when
  // AllowToPurgeSeedDataFromMemory() is called.
  // Note: if the seed data is empty or kIdenticalToSafeSeedSentinel, it
  // will be kept in memory even if this is true.
  bool seed_purgeable_from_memory_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SeedReaderWriter> weak_ptr_factory_{this};
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SEED_READER_WRITER_H_
