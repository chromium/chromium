// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SEED_STORE_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SEED_STORE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/metrics.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/seed_reader_writer.h"
#include "components/variations/seed_response.h"
#include "components/variations/variations_safe_seed_store.h"

class PrefService;
class PrefRegistrySimple;

namespace variations {

struct ClientFilterableState;
class VariationsSeed;

// A seed that has passed validation.
struct ValidatedSeed {
  ValidatedSeed();
  ~ValidatedSeed();

  // Move-only to avoid expensive copies of seed data.
  ValidatedSeed(ValidatedSeed&& other);
  ValidatedSeed& operator=(ValidatedSeed&& other);

  // Serialized VariationsSeed.
  std::string seed_data;
  // A cryptographic signature on the seed_data.
  std::string base64_seed_signature;
  // The seed data parsed as a proto.
  VariationsSeed parsed;
};

// VariationsSeedStore is a helper class for reading and writing the variations
// seed from Local State.
class COMPONENT_EXPORT(VARIATIONS) VariationsSeedStore {
 public:
  // The seed store contains two distinct seeds:
  //   (1) The most recently fetched, or "latest", seed; and
  //   (2) A "safe" seed, which has been observed to keep Chrome in a basically
  //       functional state. In particular, a safe seed is one that allows
  //       Chrome to receive new seed updates from the server.
  // Note that it's possible for both seeds to be empty, and it's possible for
  // the two seeds to be identical in their contents.
  enum class SeedType {
    LATEST,
    SAFE,
  };

  // |local_state| provides access to Local State prefs. Must not be null.
  // |initial_seed|, if not null, is stored in this seed store. It is used (A)
  // by Android Chrome and iOS to supply a first-run seed and (B) by Android
  // WebView to supply a seed on every run.
  // |signature_verification_enabled| can be used in unit tests to disable
  // signature checks on the seed.
  // |safe_seed_store| controls loading and storing safe seed data.
  // |channel| describes the release channel of the browser.
  // |seed_file_dir| is the file path to the seed file directory. If empty, the
  // seed is not stored in a separate seed file, only in |local_state_|.
  // |entropy_providers| used to provide entropy when setting up the seed file
  // field trial. If null, the client will not participate in the experiment.
  // |use_first_run_prefs|, if true (default), facilitates modifying Java
  // SharedPreferences ("first run prefs") on Android. If false,
  // SharedPreferences are not accessed.
  VariationsSeedStore(PrefService* local_state,
                      std::unique_ptr<SeedResponse> initial_seed,
                      bool signature_verification_enabled,
                      std::unique_ptr<VariationsSafeSeedStore> safe_seed_store,
                      version_info::Channel channel,
                      const base::FilePath& seed_file_dir,
                      const EntropyProviders* entropy_providers = nullptr,
                      bool use_first_run_prefs = true);

  VariationsSeedStore(const VariationsSeedStore&) = delete;
  VariationsSeedStore& operator=(const VariationsSeedStore&) = delete;

  virtual ~VariationsSeedStore();

  // Callback for loading and parsing the seed. The result should only be
  // considered valid if `success` is true.
  using LoadSeedCallback =
      base::OnceCallback<void(/*seed_data=*/std::string,
                              /*seed_signature=*/std::string,
                              /*success=*/bool,
                              /*seed=*/VariationsSeed)>;

  // Loads the latest seed and calls `done_callback` with the result. If the
  // load is unsuccessful, the prefs associated with the seed will be cleared.
  void LoadSeed(LoadSeedCallback done_callback,
                bool require_synchronous = false);

  // Loads the variations seed data into `seed`, as well as the
  // raw pref values into `seed_data` and `base64_signature`. If there is a
  // problem with loading, clears the seed pref value and returns false. If
  // successful, fills the the outparams with the loaded data and returns true.
  // Virtual for testing.
  [[nodiscard]] virtual bool LoadSeedSync(VariationsSeed* seed,
                                          std::string* seed_data,
                                          std::string* base64_seed_signature);

  // Stores the given seed |data| (serialized protobuf) to local state, along
  // with a base64-encoded digital signature for seed and the date when it was
  // fetched. If |is_gzip_compressed| is true, treats |data| as being gzip
  // compressed and decompresses it before any other processing.
  // If |is_delta_compressed| is true, treats |data| as being delta
  // compressed and attempts to decode it first using the store's seed data.
  // The actual seed data will be base64 encoded for storage. If the string
  // is invalid, the existing prefs are untouched and false is returned.
  // Additionally, stores the |country_code| that was received with the seed in
  // a separate pref. |done_callback| will be called with the result of the
  // operation, with a non-empty de-serialized, decoded protobuf VariationsSeed
  // on success. If |require_synchronous| is true, all the changes will be
  // performed synchronously, whereas otherwise some processing can be async.
  // Note: Strings are passed by value to support std::move() semantics.
  void StoreSeedData(
      base::OnceCallback<void(bool, VariationsSeed)> done_callback,
      std::string data,
      std::string base64_seed_signature,
      std::string country_code,
      base::Time date_fetched,
      bool is_delta_compressed,
      bool is_gzip_compressed,
      bool require_synchronous);

  // Loads the safe variations seed data. Returns true iff the safe seed was
  // read successfully from prefs.
  //
  // Side effect: Upon failing to read or validate the safe seed, clears all
  // of the safe seed pref values.
  void LoadSafeSeed(LoadSeedCallback done_callback,
                    bool require_synchronous = false);

  // Loads the safe variations seed data into `seed` and updates any relevant
  // fields in `client_state`. Returns true iff the safe seed was read
  // successfully from prefs. If the safe seed could not be loaded, it is
  // guaranteed that no fields in `client_state` are modified.
  //
  // Side effect: Upon failing to read or validate the safe seed, clears all
  // of the safe seed pref values.
  [[nodiscard]] virtual bool LoadSafeSeedSync(
      VariationsSeed* seed,
      ClientFilterableState* client_state);

  // Stores the given `seed_data` (a serialized protobuf) as a safe seed, along
  // with a base64-encoded digital signature for seed and any additional client
  // metadata relevant to the safe seed. Calls `done_callback` with true on
  // success or false on failure; no prefs are updated in case of failure.
  // Virtual for testing.
  virtual void StoreSafeSeed(base::OnceCallback<void(bool)> done_callback,
                             const std::string& seed_data,
                             const std::string& base64_seed_signature,
                             int seed_milestone,
                             const ClientFilterableState& client_state,
                             base::Time seed_fetch_time);

  // Loads the last fetch time (for the latest seed) that was persisted. Returns
  // base::Time() if there is no seed.
  base::Time GetLatestSeedFetchTime() const;

  // Returns the client-side timestamp at which the safe seed was fetched.
  // Returns base::Time() if there is no safe seed.
  // Virtual for early-boot CrOS experiments to use a different safe seed.
  virtual base::Time GetSafeSeedFetchTime() const;

  // Loads the milestone that was used for the latest seed that was persisted to
  // the local state.
  int GetLatestMilestone() const;

  // Returns the milestone that was used for the safe seed.
  int GetSafeSeedMilestone() const;

  // Records |fetch_time| as the last time at which a seed was fetched
  // successfully. Also updates the safe seed's fetch time if the latest and
  // safe seeds are identical.
  void RecordLastFetchTime(base::Time fetch_time);

  // Loads the last server-provided seed date (for the latest seed) that was
  // persisted to the local state.
  // (See VariationsSeedStore::GetTimeForStudyDateChecks().)
  base::Time GetLatestTimeForStudyDateChecks() const;

  // Loads the last server-provided safe seed date of when the seed to be used
  // was fetched. (See VariationsSeedStore::GetTimeForStudyDateChecks().)
  base::Time GetSafeSeedTimeForStudyDateChecks() const;

  // Returns the time to use when determining whether a client should
  // participate in a study. The returned time is one of the following:
  // (A) The server-provided timestamp of when the seed to be used was fetched.
  // (B) The Chrome binary's build time.
  // (C) A client-provided timestamp stored in prefs during the FRE on some
  //     platforms (in ChromeFeatureListCreator::SetupInitialPrefs()).
  //
  // These are prioritized as follows:
  // (1) The server-provided timestamp (A) is returned when it is available and
  //     fresher than the binary build time.
  // (2) The client-provided timestamp (C) is returned if it was written to
  //     prefs, has not yet been overwritten by a server-provided timestamp,
  //     and it is fresher than the binary build time.
  // (3) Otherwise, the binary build time (B) is returned.
  base::Time GetTimeForStudyDateChecks(bool is_safe_seed);

  // Updates |kVariationsSeedDate| and logs when previous date was from a
  // different day.
  void UpdateSeedDateAndLogDayChange(base::Time server_date_fetched);

  // Creates a histogram for the result of the update of the seed date.
  void LogSeedDayChange(base::Time server_date_fetched);

  // Returns the serial number of the most recently received seed, or an empty
  // string if there is no seed (or if it could not be read).
  // Side-effect: If there is a failure while attempting to read the latest seed
  // from prefs, clears the prefs associated with the seed.
  // Efficiency note: If code will eventually need to load the latest seed, it's
  // more efficient to call LoadSeed() prior to calling this method.
  const std::string& GetLatestSerialNumber();

  // Returns the latest country code that was received from the server.
  std::string GetLatestCountry();

  // Returns the first country code returned by the variations server after the
  // client upgraded to the version returned by
  // GetPermanentConsistencyVersion().
  std::string GetPermanentConsistencyCountry();

  // Gets the version applied to studies with permanent consistency. The version
  // at the time of storing the permanent consistency country.
  std::string GetPermanentConsistencyVersion();

  // Sets the country code and version applied to studies with permanent
  // consistency.
  void SetPermanentConsistencyCountryAndVersion(std::string_view country,
                                                std::string_view version);

  // Clears the country code and version applied to studies with permanent
  // consistency.
  void ClearPermanentConsistencyCountryAndVersion();

  PrefService* local_state() { return local_state_; }
  const PrefService* local_state() const { return local_state_; }

  // Registers Local State prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  static VerifySignatureResult VerifySeedSignatureForTesting(
      const std::string& seed_bytes,
      const std::string& base64_seed_signature);

  // Given a serialized VariationsSeed, compress it and base-64 encode it.
  // Fails if gzip encoding fails.
  static std::optional<std::string> SeedBytesToCompressedBase64Seed(
      const std::string& seed_bytes);

  // Gets |seed_reader_writer_| for testing.
  SeedReaderWriter* GetSeedReaderWriterForTesting();

  // Sets |seed_reader_writer_| to the given SeedReaderWriter for testing.
  void SetSeedReaderWriterForTesting(
      std::unique_ptr<SeedReaderWriter> seed_reader_writer);

  // Gets |safe_seed_store_| SeedReaderWriter for testing.
  SeedReaderWriter* GetSafeSeedReaderWriterForTesting();

  // Sets |safe_seed_store_| SeedReaderWriter to the given one for testing.
  void SetSafeSeedReaderWriterForTesting(
      std::unique_ptr<SeedReaderWriter> seed_reader_writer);

  // Allows to remove the safe and latest seeds from memory after they have been
  // persisted to disk. This will cause next reads to potentially have to read
  // from disk.
  void AllowToPurgeSeedsDataFromMemory();

  // Calls `done_callback` with the stored seed info for debugging. Reads either
  // the latest or the safe seed, according to the specified `seed_type`.
  void GetStoredSeedInfoForDebugging(
      base::OnceCallback<void(StoredSeedInfo)> done_callback,
      SeedType seed_type);

 protected:
  // Stores the serial number of the latest seed.
  void StoreLatestSerialNumber(std::string_view serial_number);

 private:
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedStoreTest, VerifySeedSignature);
  FRIEND_TEST_ALL_PREFIXES(VariationsSeedStoreTest, ApplyDeltaPatch);

  // Move-only struct containing params related to the received variations seed.
  struct SeedData {
    std::string data;
    std::string base64_seed_signature;
    std::string country_code;
    base::Time date_fetched;
    bool is_gzip_compressed = false;
    bool is_delta_compressed = false;
    // Only set if `is_delta_compressed` is true.
    std::string existing_seed_bytes;

    SeedData();
    ~SeedData();

    // This type is move-only.
    SeedData(SeedData&& other);
    SeedData& operator=(SeedData&& other);
  };

  // The result of processing a SeedData struct.
  struct SeedProcessingResult {
    SeedData seed_data;
    StoreSeedResult result;
    // The below are only set if `result` is StoreSeedResult::kSuccess.
    ValidatedSeed validated;
    StoreSeedResult validate_result;

    SeedProcessingResult(SeedData seed_data, StoreSeedResult result);
    ~SeedProcessingResult();

    // This type is move-only.
    SeedProcessingResult(SeedProcessingResult&& other);
    SeedProcessingResult& operator=(SeedProcessingResult&& other);
  };

  // Callback for reading both seeds. They contain the result of loading the
  // safe and latest seeds.
  using ReadBothSeedsCallback = base::OnceCallback<void(
      SeedReaderWriter::ReadSeedDataResult /*safe_seed_result*/,
      SeedReaderWriter::ReadSeedDataResult /*latest_seed_result*/)>;

  // Class for reading both latest and safe seeds in parallel.
  class TwoSeedReader;

  // Clears all prefs related to variations seed storage for the specified seed
  // type.
  void ClearPrefs(SeedType seed_type);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Imports the variations seed from the Java/iOS side. Logs UMA on failure.
  // Android and iOS Chrome uses this on first run; WebView uses this on every
  // startup. In Chrome's case, it's important to set the first run seed as soon
  // as possible, because some clients query the seed store prefs directly
  // rather than accessing them via the seed store API: https://crbug.com/829527
  void ImportInitialSeed(std::unique_ptr<SeedResponse> initial_seed);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  // Reads the variations seed data from SeedReaderWriter into |seed_data|, and
  // returns the result of the load. If a pointer for the signature is provided,
  // the signature will be read and stored into |base64_seed_signature|. The
  // value stored into |seed_data| should only be used if the result is SUCCESS.
  // Reads either the latest or the safe seed, according to the specified
  // |seed_type|. Side-effect: If the read fails, clears the prefs associated
  // with the seed.
  [[nodiscard]] LoadSeedResult ReadSeedData(
      SeedType seed_type,
      std::string* seed_data,
      std::string* base64_seed_signature = nullptr);

  // Same as above, but allows for asynchronous reads if the seed has to be read
  // from the seed file. Synchronous reads should only be used on startup, since
  // it may block the UI thread. The |done_callback| will be called when the
  // read is complete. The result of the read will be handled by
  // VariationsSeedStore::ReadSeedDataCallback().
  void ReadSeedData(SeedReaderWriter::ReadSeedDataCallback done_callback,
                    SeedType seed_type,
                    bool require_synchronous);

  // Callback for VariationsSeedStore::ReadSeedData(). If the read was
  // successful, it will call the |done_callback| with the result of the read
  // and the seed data. If the read was unsuccessful, it will clear the prefs
  // associated with the seed and call the |done_callback| with empty seed data.
  void CheckReadSeedDataResultAndRunCallback(
      SeedReaderWriter::ReadSeedDataCallback done_callback,
      SeedType seed_type,
      bool require_synchronous,
      SeedReaderWriter::ReadSeedDataResult read_result);

  // Processes the seed data (decompression, parsing and signature
  // verification) and stores the result.
  void ProcessAndStoreSeedData(
      base::OnceCallback<void(bool, VariationsSeed)> done_callback,
      SeedData seed_data,
      bool require_synchronous,
      SeedReaderWriter::ReadSeedDataResult read_result);

  // Called on the UI thread after the seed has been processed.
  void OnSeedDataProcessed(
      base::OnceCallback<void(bool, VariationsSeed)> done_callback,
      bool require_synchronous,
      SeedProcessingResult result);

  // Stores the validated seed and calls `done_callback` with the result of the
  // store and the validated seed.
  void StoreValidatedSeed(
      base::OnceCallback<void(bool, VariationsSeed)> done_callback,
      ValidatedSeed seed,
      std::string country_code,
      base::Time date_fetched,
      bool require_synchronous,
      SeedReaderWriter::ReadSeedDataResult read_result);

  // Called after the safe seed has been stored. It records the result of the
  // store and calls `done_callback`.
  void OnValidatedSafeSeedStored(base::OnceCallback<void(bool)> done_callback,
                                 StoreSeedResult store_result);

  // Reads both the safe and latest seeds. The callback is called with the
  // results of each read in that order.
  void ReadBothSeedsData(ReadBothSeedsCallback done_callback);

  // Updates the safe seed with validated data.
  void StoreValidatedSafeSeed(
      base::OnceCallback<void(StoreSeedResult)> done_callback,
      ValidatedSeed seed,
      int seed_milestone,
      base::Time reference_date,
      std::string session_consistency_country,
      std::string permanent_consistency_country,
      std::string locale,
      base::Time seed_fetch_time,
      SeedReaderWriter::ReadSeedDataResult safe_seed_read_result,
      SeedReaderWriter::ReadSeedDataResult latest_seed_read_result);

  // Processes seed data (decompression, parsing and signature verification).
  // This is meant to be called on a background thread in the case of periodic
  // seed fetches, but will also be done synchronously in the case of importing
  // a seed on startup.
  [[nodiscard]] static SeedProcessingResult ProcessSeedData(
      bool signature_verification_enabled,
      SeedData seed_data);

  // Validates that |seed_bytes| parses and matches |base64_seed_signature|.
  // Signature checking may be disabled via |signature_verification_enabled|.
  // |seed_type| indicates the source of the seed for logging purposes.
  // |result| must be non-null, and will be populated on success.
  // Returns success or some error value.
  [[nodiscard]] static StoreSeedResult ValidateSeedBytes(
      const std::string& seed_bytes,
      const std::string& base64_seed_signature,
      SeedType seed_type,
      bool signature_verification_enabled,
      ValidatedSeed* result);

  // Applies a delta-compressed |patch| to |existing_data|, producing the result
  // in |output|. Returns whether the operation was successful.
  [[nodiscard]] static bool ApplyDeltaPatch(const std::string& existing_data,
                                            const std::string& patch,
                                            std::string* output);

  // Verify an already-loaded `seed_data` along with its `base64_seed_signature`
  // and, if verification passes, parse it into `*seed`.
  [[nodiscard]] LoadSeedResult VerifyAndParseSeedImpl(
      VariationsSeed* seed,
      const std::string& seed_data,
      const std::string& base64_seed_signature,
      std::optional<VerifySignatureResult>* verify_signature_result);

  // Verifies the seed from `read_result`, parses it into a VariationsSeed and
  // calls `done_callback` with the result.
  void VerifyAndParseSeedAndRunCallback(
      LoadSeedCallback done_callback,
      SeedType seed_type,
      SeedReaderWriter::ReadSeedDataResult read_result);

  // Logs the result of loading the seed.
  void LogLoadSeedResult(SeedType seed_type, LoadSeedResult result);

  // The pref service used to persist the variations seed.
  raw_ptr<PrefService> local_state_;

  // Setters and getters for safe seed state.
  std::unique_ptr<VariationsSafeSeedStore> safe_seed_store_;

  // Whether to validate signatures on the seed. Always on except in unit tests.
  const bool signature_verification_enabled_;

  // Whether this may read or write to Java "first run" SharedPreferences.
  const bool use_first_run_prefs_;

  // Handles reads and writes to seed files.
  std::unique_ptr<SeedReaderWriter> seed_reader_writer_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<VariationsSeedStore> weak_ptr_factory_{this};
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SEED_STORE_H_
