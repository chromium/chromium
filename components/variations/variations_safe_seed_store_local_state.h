// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SAFE_SEED_STORE_LOCAL_STATE_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SAFE_SEED_STORE_LOCAL_STATE_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/version_info/channel.h"
#include "components/variations/seed_reader_writer.h"
#include "components/variations/variations_safe_seed_store.h"

class PrefService;
class PrefRegistrySimple;

namespace variations {

class EntropyProviders;

// Implementation of VariationsSafeSeedStore that uses local state and the
// standard prefs from it.
class COMPONENT_EXPORT(VARIATIONS) VariationsSafeSeedStoreLocalState
    : public VariationsSafeSeedStore {
 public:
  // |local_state| should generally be the same value that VariationsSeedStore
  // uses.
  // |seed_file_dir| is the file path to the seed file directory. If empty, the
  // seed is not stored in a separate seed file, only in |local_state_|.
  // |channel| describes the release channel of the browser.
  // |entropy_providers| is used to provide entropy when setting up the seed
  // file field trial. If null, the client will not participate in the
  // experiment.
  explicit VariationsSafeSeedStoreLocalState(
      PrefService* local_state,
      const base::FilePath& seed_file_dir,
      version_info::Channel channel,
      const EntropyProviders* entropy_providers);

  VariationsSafeSeedStoreLocalState(const VariationsSafeSeedStoreLocalState&) =
      delete;
  VariationsSafeSeedStoreLocalState& operator=(
      const VariationsSafeSeedStoreLocalState&) = delete;

  ~VariationsSafeSeedStoreLocalState() override;

  // VariationsSafeSeedStore:
  base::Time GetFetchTime() const override;
  void SetFetchTime(const base::Time& fetch_time) override;
  int GetMilestone() const override;
  base::Time GetTimeForStudyDateChecks() const override;
  StoreSeedResult SetCompressedSeed(ValidatedSeedInfo seed_info) override;
  std::string GetLocale() const override;
  void SetLocale(const std::string& locale) override;
  std::string GetPermanentConsistencyCountry() const override;
  std::string GetSessionConsistencyCountry() const override;
  SeedReaderWriter* GetSeedReaderWriterForTesting() override;
  void SetSeedReaderWriterForTesting(
      std::unique_ptr<SeedReaderWriter> seed_reader_writer) override;
  void ClearState() override;
  LoadSeedResult ReadSeedData(std::string* seed_data,
                              std::string* base64_seed_signature) override;
  void ReadSeedData(
      SeedReaderWriter::ReadSeedDataCallback done_callback) override;
  void AllowToPurgeSeedDataFromMemory() override;
  void GetStoredSeedInfoForDebugging(
      base::OnceCallback<void(StoredSeedInfo)> done_callback) override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Local State accessor, which should be the same as the one in
  // VariationsSeedStore.
  raw_ptr<PrefService> local_state_;

  // Handles reads and writes to seed files.
  std::unique_ptr<SeedReaderWriter> seed_reader_writer_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SAFE_SEED_STORE_LOCAL_STATE_H_
