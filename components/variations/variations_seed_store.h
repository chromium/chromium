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
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/variations/metrics.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/seed_response.h"
#include "components/variations/variations_safe_seed_store.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class PrefService;
class PrefRegistrySimple;

namespace variations {

struct ClientFilterableState;
class VariationsSeed;

// A seed that has passed validation.
struct ValidatedSeed {
  // Gzipped and base-64 encoded serialized VariationsSeed.
  std::string base64_seed_data;
  // A cryptographic signature on the seed_data.
  std::string base64_seed_signature;
  // The seed data parsed as a proto.
  VariationsSeed parsed;
};

// VariationsSeedStore is a helper class for reading and writing the variations
// seed from Local State.
class COMPONENT_EXPORT(VARIATIONS) VariationsSeedStore {
 public:
  // Standard constructor. Enables signature verification.
  // |safe_seed_store| controls how to load and store the safe seed data.
  // TODO(crbug.com/40935052): Remove this constructor and migrate
  // callers to the more-verbose version.
  VariationsSeedStore(PrefService* local_state,
                      std::unique_ptr<VariationsSafeSeedStore> safe_seed_store);

  // |local_state| provides access to Local State prefs. Must not be null.
  // |initial_seed|, if not null, is stored in this seed store. It is used (A)
  // by Android Chrome and iOS to supply a first-run seed and (B) by Android
  // WebView to supply a seed on every run.
  // |signature_verification_enabled| can be used in unit tests to disable
  // signature checks on the seed.
  // |safe_seed_store| controls loading and storing safe seed data.
  // |use_first_run_prefs|, if true (default), facilitates modifying Java
  // SharedPreferences ("first run prefs") on Android. If false,
  // SharedPreferences are not accessed.
  VariationsSeedStore(PrefService* local_state,
                      std::unique_ptr<SeedResponse> initial_seed,
                      bool signature_verification_enabled,
                      std::unique_ptr<VariationsSafeSeedStore> safe_seed_store,
                      bool use_first_run_prefs = true);

  VariationsSeedStore(const VariationsSeedStore&) = delete;
  VariationsSeedStore& operator=(const VariationsSeedStore&) = delete;

  virtual ~VariationsSeedStore();

  // Loads the variations seed data from local state into |seed|, as well as the
  // raw pref values into |seed_data| and |base64_signature|. If there is a
  // problem with loading, clears the seed pref value and returns false. If
  // successful, fills the the outparams with the loaded data and returns true.
  // Virtual for testing.
  [[nodiscard]] virtual bool LoadSeed(VariationsSeed* seed,
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
      std::string data,
      std::string base64_seed_signature,
      std::string country_code,
      base::Time date_fetched,
      bool is_delta_compressed,
      bool is_gzip_compressed,
      base::OnceCallback<void(bool, VariationsSeed)> done_callback,
      bool require_synchronous = false);

  // Loads the safe variations seed data from local state into |seed| and
  // updates any relevant fields in |client_state|. Returns true iff the safe
  // seed was read successfully from prefs. If the safe seed could not be
  // loaded, it is guaranteed that no fields in |client_state| are modified.
  //
  // Side effect: Upon failing to read or validate the safe seed, clears all
  // of the safe seed pref values.
  //
  // Virtual for testing and for early-boot CrOS experiments to use a different
  // safe seed.
  [[nodiscard]] virtual bool LoadSafeSeed(VariationsSeed* seed,
                                          ClientFilterableState* client_state);

  // Stores the given |seed_data| (a serialized protobuf) to local state as a
  // safe seed, along with a base64-encoded digital signature for seed and any
  // additional client metadata relevant to the safe seed. Returns true on
  // success or false on failure; no prefs are updated in case of failure.
  // Virtual for testing.
  virtual bool StoreSafeSeed(const std::string& seed_data,
                             const std::string& base64_seed_signature,
                             int seed_milestone,
                             const ClientFilterableState& client_state,
                             base::Time seed_fetch_time);

  // Loads the last fetch time (for the latest seed) that was persisted to the
  // local state.
  base::Time GetLastFetchTime() const;

  // Returns the time at which the safe seed was persisted to the local state.
  //
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
  // persisted to the local state. (See GetTimeForStudyDateChecks.)
  base::Time GetLatestTimeForStudyDateChecks() const;

  // Loads the last server-provided safe seed date of when the seed to be used
  // was fetched. (See GetTimeForStudyDateChecks.)
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

  // Returns the serial number of the most recently received seed, or an empty
  // string if there is no seed (or if it could not be read).
  // Side-effect: If there is a failure while attempting to read the latest seed
  // from prefs, clears the prefs associated with the seed.
  // Efficiency note: If code will eventually need to load the latest seed, it's
  // more efficient to call LoadSeed() prior to calling this method.
  const std::string& GetLatestSerialNumber();

  PrefService* local_state() { return local_state_; }
  const PrefService* local_state() const { return local_state_; }

  // Registers Local State prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Loads the last fetch time (for the latest seed) that was persisted to
  // |local_state|.
  static base::Time GetLastFetchTimeFromPrefService(PrefService* local_state);

  static VerifySignatureResult VerifySeedSignatureForTesting(
      const std::string& seed_bytes,
      const std::string& base64_seed_signature);

  // Given a serialized VariationsSeed, compress it and base-64 encode it.
  // Fails if gzip encoding fails.
  static std::optional<std::string> SeedBytesToCompressedBase64Seed(
      const std::string& seed_bytes);

 protected:
  // Verify an already-loaded |seed_data| along with its |base64_seed_signature|
  // and, if verification passes, parse it into |*seed|.
  [[nodiscard]] LoadSeedResult VerifyAndParseSeed(
      VariationsSeed* seed,
      const std::string& seed_data,
      const std::string& base64_seed_signature,
      std::optional<VerifySignatureResult>* verify_signature_result);

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

  // Loads the variations seed data from local state into |seed|, as well as the
  // raw pref values into |seed_data| and |base64_signature|. Loads either the
  // safe seed or the latest seed, according to the |seed_type|. Returns whether
  // loading the seed was successful.
  // Side-effect: Upon any failure to read or validate the safe seed, clears all
  // of the pref values for the seed. This occurs iff the method returns false.
  [[nodiscard]] LoadSeedResult LoadSeedImpl(SeedType seed_type,
                                            VariationsSeed* seed,
                                            std::string* seed_data,
                                            std::string* base64_seed_signature);

  // Reads the variations seed data from prefs into |seed_data|, and returns the
  // result of the load. The value stored into |seed_data| should only be used
  // if the result is SUCCESS. Reads either the latest or the safe seed,
  // according to the specified |seed_type|.
  // Side-effect: If the read fails, clears the prefs associated with the seed.
  [[nodiscard]] LoadSeedResult ReadSeedData(SeedType seed_type,
                                            std::string* seed_data);

  // Resolves a |delta_bytes| against the latest seed.
  // Returns success or an error, populating |seed_bytes| on success.
  [[nodiscard]] StoreSeedResult ResolveDelta(const std::string& delta_bytes,
                                             std::string* seed_bytes);

  // Resolves instance manipulations applied to received data.
  // Returns success or an error, populating |seed_bytes| on success.
  [[nodiscard]] StoreSeedResult ResolveInstanceManipulations(
      const std::string& data,
      const InstanceManipulations& im,
      std::string* seed_bytes);

  // Called on the UI thread after the seed has been processed.
  void OnSeedDataProcessed(
      base::OnceCallback<void(bool, VariationsSeed)> done_callback,
      SeedProcessingResult result);

  // Updates the latest seed with validated data.
  void StoreValidatedSeed(const ValidatedSeed& seed,
                          const std::string& country_code,
                          base::Time date_fetched);

  // Updates the safe seed with validated data.
  void StoreValidatedSafeSeed(const ValidatedSeed& seed,
                              int seed_milestone,
                              const ClientFilterableState& client_state,
                              base::Time seed_fetch_time);

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

  // The pref service used to persist the variations seed.
  raw_ptr<PrefService> local_state_;

  // Setters and getters for safe seed state.
  std::unique_ptr<VariationsSafeSeedStore> safe_seed_store_;

  // Cached serial number from the most recently fetched variations seed.
  std::string latest_serial_number_;

  // Whether to validate signatures on the seed. Always on except in unit tests.
  const bool signature_verification_enabled_;

  // Whether this may read or write to Java "first run" SharedPreferences.
  const bool use_first_run_prefs_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Gets the combined server and client state used for early boot variations
  // platform disaster recovery.
  featured::SeedDetails GetSafeSeedStateForPlatform(
      const ValidatedSeed& seed,
      const int seed_milestone,
      const ClientFilterableState& client_state,
      const base::Time seed_fetch_time);

  // Retries sending the safe seed to platform. Does not retry after two failed
  // attempts.
  void MaybeRetrySendSafeSeed(const featured::SeedDetails& safe_seed,
                              bool success);

  // Sends the safe seed to the platform.
  void SendSafeSeedToPlatform(const featured::SeedDetails& safe_seed);

  // A counter that keeps track of how many times the current safe seed is sent
  // to platform.
  size_t send_seed_to_platform_attempts_ = 0;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<VariationsSeedStore> weak_ptr_factory_{this};
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SEED_STORE_H_
