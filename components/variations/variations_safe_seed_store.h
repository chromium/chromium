// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SAFE_SEED_STORE_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SAFE_SEED_STORE_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/version_info/channel.h"
#include "components/variations/seed_reader_writer.h"

class PrefService;
class PrefRegistrySimple;

namespace variations {

class EntropyProviders;

// Encapsulates details of reading and modifying safe seed state.
class COMPONENT_EXPORT(VARIATIONS) VariationsSafeSeedStore {
 public:
  // |local_state| should generally be the same value that VariationsSeedStore
  // uses.
  // |seed_file_dir| is the file path to the seed file directory. If empty, the
  // seed is not stored in a separate seed file, only in |local_state|.
  // |channel| describes the release channel of the browser.
  // |entropy_providers| is used to provide entropy when setting up the seed
  // file field trial. If null, the client will not participate in the
  // experiment.
  explicit VariationsSafeSeedStore(PrefService* local_state,
                                   const base::FilePath& seed_file_dir,
                                   version_info::Channel channel,
                                   const EntropyProviders* entropy_providers);

  VariationsSafeSeedStore(const VariationsSafeSeedStore&) = delete;
  VariationsSafeSeedStore& operator=(const VariationsSafeSeedStore&) = delete;

  ~VariationsSafeSeedStore();

  // Getter and setter for the time at which the safe seed was persisted to the
  // underlying storage.
  base::Time GetFetchTime() const;
  void SetFetchTime(const base::Time& fetch_time);

  // Getter for the milestone that was used for the safe seed.
  int GetMilestone() const;

  // Getter for the last server-provided safe seed date of when the seed to be
  // used was fetched. (See VariationsSeedStore::GetTimeForStudyDateChecks().)
  base::Time GetTimeForStudyDateChecks() const;

  // Setter for the safe seed and other seed-related info.
  StoreSeedResult SetCompressedSeed(ValidatedSeedInfo seed_info);

  // Getter and setter for the locale associated with the safe seed in the
  // underlying storage.
  std::string GetLocale() const;
  void SetLocale(const std::string& locale);

  // Getter for the permanent consistency country associated with the safe seed
  // in the underlying storage.
  std::string GetPermanentConsistencyCountry() const;

  // Getter for the session consistency country associated with the safe seed in
  // the underlying storage.
  std::string GetSessionConsistencyCountry() const;

  // Getter and setter for SeedReaderWriter for testing.
  SeedReaderWriter* GetSeedReaderWriterForTesting();
  void SetSeedReaderWriterForTesting(
      std::unique_ptr<SeedReaderWriter> seed_reader_writer);

  // Clear all state in the underlying storage.
  void ClearState();

  // Reads seed data and returns the result of the load. If a pointer for the
  // signature is provided, the signature will be read and stored into
  // |base64_seed_signature|. The value stored into |seed_data| should only be
  // used if the result is `LoadSeedResult::kSuccess`.
  // Side-effect: If the read fails, clears the prefs associated with the seed.
  LoadSeedResult ReadSeedData(std::string* seed_data,
                              std::string* base64_seed_signature);

  // Reads and processes seed data and calls `done_callback` with the result of
  // the load, the seed data, and the signature. The seed data and signature
  // should only be used if the result is `LoadSeedResult::kSuccess`.
  // Side-effect: If the read fails, clears the prefs associated with the seed.
  void ReadSeedData(SeedReaderWriter::ReadSeedDataCallback done_callback);

  // Allows the safe seed to be purged from memory after being persisted. This
  // will cause next reads to potentially have to read from disk.
  void AllowToPurgeSeedDataFromMemory();

  // Calls `done_callback` with the stored seed info for debugging.
  void GetStoredSeedInfoForDebugging(
      base::OnceCallback<void(StoredSeedInfo)> done_callback);

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Local State accessor.
  raw_ptr<PrefService> local_state_;

  // Handles reads and writes to seed files.
  std::unique_ptr<SeedReaderWriter> seed_reader_writer_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SAFE_SEED_STORE_H_
