// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_store.h"

#include <stdint.h>

#include <utility>

#include "base/base64.h"
#include "base/build_time.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/seed_reader_writer.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "crypto/signature_verifier.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/zlib/google/compression_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#include "components/variations/metrics.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
#include "components/variations/metrics.h"
#endif  // BUILDFLAG(IS_IOS)

namespace variations {
namespace {

// The ECDSA public key of the variations server for verifying variations seed
// signatures.
const uint8_t kPublicKey[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0x51, 0x7c, 0x31, 0x4b, 0x50, 0x42, 0xdd, 0x59, 0xda,
    0x0b, 0xfa, 0x43, 0x44, 0x33, 0x7c, 0x5f, 0xa1, 0x0b, 0xd5, 0x82, 0xf6,
    0xac, 0x04, 0x19, 0x72, 0x6c, 0x40, 0xd4, 0x3e, 0x56, 0xe2, 0xa0, 0x80,
    0xa0, 0x41, 0xb3, 0x23, 0x7b, 0x71, 0xc9, 0x80, 0x87, 0xde, 0x35, 0x0d,
    0x25, 0x71, 0x09, 0x7f, 0xb4, 0x15, 0x2b, 0xff, 0x82, 0x4d, 0xd3, 0xfe,
    0xc5, 0xef, 0x20, 0xc6, 0xa3, 0x10, 0xbf,
};

// LINT.IfChange
// The name of the seed file that stores the latest seed data and other
// seed-related information in a compressed proto.
const base::FilePath::CharType kSeedFilename[] =
    FILE_PATH_LITERAL("VariationsSeedV2");
// LINT.ThenChange(/components/variations/variations_safe_seed_store_local_state.cc,
// /chrome/browser/metrics/variations/variations_safe_mode_end_to_end_browsertest.cc)

// Name of the old seed file. It stores only the seed data gzip-compressed.
// TODO(crbug.com/411431524): Remove this once the experiment has ended.
const base::FilePath::CharType kOldSeedFilename[] =
    FILE_PATH_LITERAL("VariationsSeedV1");

// Returns true if |signature| is empty and if the command-line flag to accept
// empty seed signature is specified.
bool AcceptEmptySeedSignatureForTesting(const std::string& signature) {
  return signature.empty() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAcceptEmptySeedSignatureForTesting);
}

// Verifies a variations seed (the serialized proto bytes) with the specified
// base-64 encoded signature that was received from the server and returns the
// result. The signature is assumed to be an "ECDSA with SHA-256" signature
// (see kECDSAWithSHA256AlgorithmID in the .cc file). Returns the result of
// signature verification.
VerifySignatureResult VerifySeedSignature(
    const std::string& seed_bytes,
    const std::string& base64_seed_signature) {
  if (base64_seed_signature.empty()) {
    return VerifySignatureResult::kMissingSignature;
  }

  std::string signature;
  if (!base::Base64Decode(base64_seed_signature, &signature))
    return VerifySignatureResult::kDecodeFailed;

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(crypto::SignatureVerifier::ECDSA_SHA256,
                           base::as_byte_span(signature), kPublicKey)) {
    return VerifySignatureResult::kInvalidSignature;
  }

  verifier.VerifyUpdate(base::as_byte_span(seed_bytes));
  if (!verifier.VerifyFinal()) {
    return VerifySignatureResult::kInvalidSeed;
  }

  return VerifySignatureResult::kValidSignature;
}

// Truncates a time to the start of the day in UTC. If given a time representing
// 2014-03-11 10:18:03.1 UTC, it will return a time representing
// 2014-03-11 00:00:00.0 UTC.
base::Time TruncateToUTCDay(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  base::Time out_time;
  bool conversion_success = base::Time::FromUTCExploded(exploded, &out_time);
  DCHECK(conversion_success);
  return out_time;
}

UpdateSeedDateResult GetSeedDateChangeState(
    base::Time server_seed_date,
    base::Time stored_seed_date) {
  if (server_seed_date < stored_seed_date) {
    return UpdateSeedDateResult::kNewDateIsOlder;
  }

  if (TruncateToUTCDay(server_seed_date) !=
      TruncateToUTCDay(stored_seed_date)) {
    // The server date is later than the stored date, and they are from
    // different UTC days, so |server_seed_date| is a valid new day.
    return UpdateSeedDateResult::kNewDay;
  }
  return UpdateSeedDateResult::kSameDay;
}

// Remove gzip compression from |data|.
// Returns success or error, populating result on success.
StoreSeedResult Uncompress(const std::string& compressed, std::string* result) {
  DCHECK(result);
  if (!compression::GzipUncompress(compressed, result)) {
    return StoreSeedResult::kFailedUngzip;
  }
  if (result->empty()) {
    return StoreSeedResult::kFailedEmptyGzipContents;
  }
  return StoreSeedResult::kSuccess;
}

#if BUILDFLAG(IS_ANDROID)
// Marks seed storing as successful on the Java side to avoid repeated seed
// fetches. Called only on first run.
void MarkVariationsSeedAsStoredIfEmptySeed(
    SeedReaderWriter::ReadSeedDataResult read_result) {
  if (read_result.result == LoadSeedResult::kEmpty) {
    android::MarkVariationsSeedAsStored();
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

class VariationsSeedStore::TwoSeedReader
    : public base::RefCountedThreadSafe<TwoSeedReader> {
 public:
  explicit TwoSeedReader(
      base::OnceCallback<void(SeedReaderWriter::ReadSeedDataResult,
                              SeedReaderWriter::ReadSeedDataResult)>
          done_callback)
      : done_callback_(std::move(done_callback)) {}

  // Called when a single seed has been read. If both seeds have been read,
  // `done_callback_` will be called.
  void OnSingleSeedRead(SeedType seed_type,
                        SeedReaderWriter::ReadSeedDataResult read_result) {
    switch (seed_type) {
      case SeedType::SAFE:
        safe_seed_read_result_ = std::move(read_result);
        break;
      case SeedType::LATEST:
        latest_seed_read_result_ = std::move(read_result);
        break;
    }
    if (IsDone()) {
      std::move(done_callback_)
          .Run(std::move(safe_seed_read_result_).value(),
               std::move(latest_seed_read_result_).value());
    }
  }

 private:
  friend class base::RefCountedThreadSafe<TwoSeedReader>;

  ~TwoSeedReader() = default;

  // Returns true if both seeds have been read. This is used to determine when
  // to call `done_callback_`.
  bool IsDone() const {
    return safe_seed_read_result_.has_value() &&
           latest_seed_read_result_.has_value();
  }

  ReadBothSeedsCallback done_callback_;

  std::optional<SeedReaderWriter::ReadSeedDataResult> safe_seed_read_result_;
  std::optional<SeedReaderWriter::ReadSeedDataResult> latest_seed_read_result_;
};

ValidatedSeed::ValidatedSeed() = default;
ValidatedSeed::~ValidatedSeed() = default;
ValidatedSeed::ValidatedSeed(ValidatedSeed&& other) = default;
ValidatedSeed& ValidatedSeed::operator=(ValidatedSeed&& other) = default;

VariationsSeedStore::VariationsSeedStore(
    PrefService* local_state,
    std::unique_ptr<SeedResponse> initial_seed,
    bool signature_verification_enabled,
    std::unique_ptr<VariationsSafeSeedStore> safe_seed_store,
    version_info::Channel channel,
    const base::FilePath& seed_file_dir,
    const EntropyProviders* entropy_providers,
    bool use_first_run_prefs)
    : local_state_(local_state),
      safe_seed_store_(std::move(safe_seed_store)),
      signature_verification_enabled_(signature_verification_enabled),
      use_first_run_prefs_(use_first_run_prefs),
      seed_reader_writer_(
          std::make_unique<SeedReaderWriter>(local_state,
                                             seed_file_dir,
                                             kSeedFilename,
                                             kOldSeedFilename,
                                             kRegularSeedFieldsPrefs,
                                             channel,
                                             entropy_providers,
                                             /*histogram_suffix=*/"Latest")) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  if (initial_seed)
    ImportInitialSeed(std::move(initial_seed));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

VariationsSeedStore::~VariationsSeedStore() = default;

void VariationsSeedStore::LoadSeed(LoadSeedCallback done_callback,
                                   bool require_synchronous) {
  auto verify_and_parse_seed_cb =
      base::BindOnce(&VariationsSeedStore::VerifyAndParseSeedAndRunCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done_callback),
                     SeedType::LATEST);
  ReadSeedData(std::move(verify_and_parse_seed_cb), SeedType::LATEST,
               require_synchronous);
}

bool VariationsSeedStore::LoadSeedSync(VariationsSeed* seed,
                                       std::string* seed_data,
                                       std::string* base64_seed_signature) {
  std::optional<bool> success;
  // It's safe to pass pointers here because the callback runs synchronously.
  LoadSeed(base::BindOnce(
               [](std::string* seed_data, std::string* seed_signature,
                  std::optional<bool>* success, VariationsSeed* seed,
                  std::string cb_seed_data, std::string cb_seed_signature,
                  bool cb_success, VariationsSeed cb_seed) {
                 *success = cb_success;
                 *seed = std::move(cb_seed);
                 *seed_data = std::move(cb_seed_data);
                 *seed_signature = std::move(cb_seed_signature);
               },
               seed_data, base64_seed_signature, &success, seed),
           /*require_synchronous=*/true);
  CHECK(success.has_value())
      << "LoadSeed callback should have run synchronously.";

  if (!success.value()) {
    return false;
  }

  // TODO(crbug.com/437811262): Remove after milestone M146. This code is used
  // to populate the pref with the serial number of the latest seed. This value
  // is already stored when we fetch a new seed, so we don't need to store it
  // again here.
  StoreLatestSerialNumber(seed->serial_number());
  return true;
}

void VariationsSeedStore::StoreSeedData(
    base::OnceCallback<void(bool, VariationsSeed)> done_callback,
    std::string data,
    std::string base64_seed_signature,
    std::string country_code,
    base::Time date_fetched,
    bool is_delta_compressed,
    bool is_gzip_compressed,
    bool require_synchronous) {
  base::ScopedUmaHistogramTimer store_seed_timer("Variations.StoreSeed.Time");

  base::UmaHistogramCounts1000("Variations.StoreSeed.DataSize",
                               data.length() / 1024);
  InstanceManipulations im = {
      .gzip_compressed = is_gzip_compressed,
      .delta_compressed = is_delta_compressed,
  };
  RecordSeedInstanceManipulations(im);

  // Note: SeedData is move-only, so it will be moved into a param below.
  SeedData seed_data;
  seed_data.data = std::move(data);
  seed_data.base64_seed_signature = std::move(base64_seed_signature);
  seed_data.country_code = std::move(country_code);
  seed_data.date_fetched = date_fetched;
  seed_data.is_gzip_compressed = is_gzip_compressed;
  seed_data.is_delta_compressed = is_delta_compressed;

  if (is_delta_compressed) {
    ReadSeedData(
        /*done_callback=*/base::BindOnce(
            &VariationsSeedStore::ProcessAndStoreSeedData,
            weak_ptr_factory_.GetWeakPtr(), std::move(done_callback),
            std::move(seed_data), require_synchronous),
        SeedType::LATEST, require_synchronous);
  } else {
    ProcessAndStoreSeedData(
        std::move(done_callback), std::move(seed_data), require_synchronous,
        SeedReaderWriter::ReadSeedDataResult{LoadSeedResult::kSuccess, "", ""});
  }
}

void VariationsSeedStore::LoadSafeSeed(LoadSeedCallback done_callback,
                                       bool require_synchronous) {
  auto verify_and_parse_seed_cb = base::BindOnce(
      &VariationsSeedStore::VerifyAndParseSeedAndRunCallback,
      weak_ptr_factory_.GetWeakPtr(), std::move(done_callback), SeedType::SAFE);
  ReadSeedData(std::move(verify_and_parse_seed_cb), SeedType::SAFE,
               require_synchronous);
}

bool VariationsSeedStore::LoadSafeSeedSync(
    VariationsSeed* seed,
    ClientFilterableState* client_state) {
  std::string unused_seed_data;
  std::string unused_base64_seed_signature;
  std::optional<bool> success;
  LoadSafeSeed(
      base::BindOnce(
          [](std::string* seed_data, std::string* seed_signature,
             std::optional<bool>* success, VariationsSeed* seed,
             std::string cb_seed_data, std::string cb_seed_signature,
             bool cb_success, VariationsSeed cb_seed) {
            *success = cb_success;
            *seed = std::move(cb_seed);
            *seed_data = std::move(cb_seed_data);
            *seed_signature = std::move(cb_seed_signature);
          },
          &unused_seed_data, &unused_base64_seed_signature, &success, seed),
      /*require_synchronous=*/true);
  CHECK(success.has_value())
      << "LoadSafeSeed callback should have run synchronously.";
  if (!success.value()) {
    return false;
  }

  // TODO(crbug.com/40202311): While it's not immediately obvious,
  // |client_state| is not used for successfully loaded safe seeds that are
  // rejected after additional validation (expiry and future milestone).
  client_state->reference_date =
      GetTimeForStudyDateChecks(/*is_safe_seed=*/true);
  client_state->locale = safe_seed_store_->GetLocale();
  client_state->permanent_consistency_country =
      safe_seed_store_->GetPermanentConsistencyCountry();
  client_state->session_consistency_country =
      safe_seed_store_->GetSessionConsistencyCountry();
  return true;
}

void VariationsSeedStore::StoreSafeSeed(
    base::OnceCallback<void(bool)> done_callback,
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    int seed_milestone,
    const ClientFilterableState& client_state,
    base::Time seed_fetch_time) {
  ValidatedSeed seed;
  // TODO(crbug.com/40839193): See if we can avoid calling this on the UI
  // thread.
  StoreSeedResult validation_result =
      ValidateSeedBytes(seed_data, base64_seed_signature, SeedType::SAFE,
                        signature_verification_enabled_, &seed);
  if (validation_result != StoreSeedResult::kSuccess) {
    RecordStoreSafeSeedResult(validation_result);
    std::move(done_callback).Run(false);
    return;
  }

  auto on_validated_safe_seed_stored_cb =
      base::BindOnce(&VariationsSeedStore::OnValidatedSafeSeedStored,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done_callback));

  auto store_validated_safe_seed_cb = base::BindOnce(
      &VariationsSeedStore::StoreValidatedSafeSeed,
      weak_ptr_factory_.GetWeakPtr(),
      /*done_callback=*/std::move(on_validated_safe_seed_stored_cb),
      std::move(seed), seed_milestone, client_state.reference_date,
      client_state.session_consistency_country,
      client_state.permanent_consistency_country, client_state.locale,
      seed_fetch_time);

  // Read both seeds and call VariationsSeedStore::StoreValidatedSafeSeed()
  ReadBothSeedsData(std::move(store_validated_safe_seed_cb));
}

void VariationsSeedStore::ReadBothSeedsData(
    ReadBothSeedsCallback done_callback) {
  auto two_seed_reader =
      base::MakeRefCounted<TwoSeedReader>(std::move(done_callback));
  ReadSeedData(
      /*done_callback=*/base::BindOnce(&TwoSeedReader::OnSingleSeedRead,
                                       two_seed_reader, SeedType::SAFE),
      SeedType::SAFE,
      /*require_synchronous=*/false);
  ReadSeedData(
      /*done_callback=*/base::BindOnce(&TwoSeedReader::OnSingleSeedRead,
                                       two_seed_reader, SeedType::LATEST),
      SeedType::LATEST,
      /*require_synchronous=*/false);
}

void VariationsSeedStore::OnValidatedSafeSeedStored(
    base::OnceCallback<void(bool)> done_callback,
    StoreSeedResult validation_result) {
  RecordStoreSafeSeedResult(validation_result);
  std::move(done_callback).Run(validation_result == StoreSeedResult::kSuccess);
}

base::Time VariationsSeedStore::GetLatestSeedFetchTime() const {
  return seed_reader_writer_->GetSeedInfo().client_fetch_time;
}

base::Time VariationsSeedStore::GetSafeSeedFetchTime() const {
  return safe_seed_store_->GetFetchTime();
}

int VariationsSeedStore::GetLatestMilestone() const {
  return seed_reader_writer_->GetSeedInfo().milestone;
}

int VariationsSeedStore::GetSafeSeedMilestone() const {
  return safe_seed_store_->GetMilestone();
}

base::Time VariationsSeedStore::GetLatestTimeForStudyDateChecks()
    const {
  return seed_reader_writer_->GetSeedInfo().seed_date;
}

base::Time VariationsSeedStore::GetSafeSeedTimeForStudyDateChecks() const {
  return safe_seed_store_->GetTimeForStudyDateChecks();
}

base::Time VariationsSeedStore::GetTimeForStudyDateChecks(bool is_safe_seed) {
  base::Time seed_date = is_safe_seed ? GetSafeSeedTimeForStudyDateChecks()
                                      : GetLatestTimeForStudyDateChecks();
  const base::Time build_time = base::GetBuildTime();

  // Use the build time for date checks if either the seed date is unknown or
  // the build time is newer than the seed date.
  if (seed_date.is_null() || seed_date < build_time) {
    return build_time;
  }
  return seed_date;
}

void VariationsSeedStore::RecordLastFetchTime(base::Time fetch_time) {
  CHECK(!fetch_time.is_null()) << "Can't record null fetch time.";
  seed_reader_writer_->SetFetchTime(fetch_time);
  // If the latest and safe seeds are identical, update the fetch time for the
  // safe seed as well.
  if (seed_reader_writer_->IsIdenticalToSafeSeedSentinel()) {
    safe_seed_store_->SetFetchTime(fetch_time);
  }
}

void VariationsSeedStore::UpdateSeedDateAndLogDayChange(
    base::Time server_date_fetched) {
  LogSeedDayChange(server_date_fetched);
  seed_reader_writer_->SetSeedDate(server_date_fetched);
}

void VariationsSeedStore::LogSeedDayChange(
    base::Time server_date_fetched) {
  UpdateSeedDateResult result = UpdateSeedDateResult::kNoOldDate;
  const base::Time stored_date = seed_reader_writer_->GetSeedInfo().seed_date;
  if (!stored_date.is_null()) {
    result = GetSeedDateChangeState(server_date_fetched, stored_date);
  }

  base::UmaHistogramEnumeration("Variations.SeedDateChange", result);
}

const std::string& VariationsSeedStore::GetLatestSerialNumber() {
  return local_state_->GetString(prefs::kVariationsSeedSerialNumber);
}

std::string VariationsSeedStore::GetLatestCountry() {
  return std::string(seed_reader_writer_->GetSeedInfo().session_country_code);
}

std::string VariationsSeedStore::GetPermanentConsistencyCountry() {
  return std::string(seed_reader_writer_->GetSeedInfo().permanent_country_code);
}

std::string VariationsSeedStore::GetPermanentConsistencyVersion() {
  return std::string(
      seed_reader_writer_->GetSeedInfo().permanent_country_version);
}

void VariationsSeedStore::ClearPermanentConsistencyCountryAndVersion() {
  seed_reader_writer_->ClearPermanentConsistencyCountryAndVersion();
}

void VariationsSeedStore::SetPermanentConsistencyCountryAndVersion(
    const std::string_view country,
    const std::string_view version) {
  seed_reader_writer_->SetPermanentConsistencyCountryAndVersion(country,
                                                                version);
}

// static
void VariationsSeedStore::RegisterPrefs(PrefRegistrySimple* registry) {
  // Regular seed prefs:
  registry->RegisterStringPref(prefs::kVariationsCompressedSeed, std::string());
  registry->RegisterStringPref(prefs::kVariationsCountry, std::string());
  registry->RegisterTimePref(prefs::kVariationsLastFetchTime, base::Time());
  registry->RegisterIntegerPref(prefs::kVariationsSeedMilestone, 0);
  registry->RegisterTimePref(prefs::kVariationsSeedDate, base::Time());
  registry->RegisterStringPref(prefs::kVariationsSeedSignature, std::string());
  // This preference keeps track of the country code used to filter
  // permanent-consistency studies.
  registry->RegisterListPref(prefs::kVariationsPermanentConsistencyCountry);
  registry->RegisterStringPref(prefs::kVariationsSeedSerialNumber,
                               std::string());

  VariationsSafeSeedStoreLocalState::RegisterPrefs(registry);
}

// static
VerifySignatureResult VariationsSeedStore::VerifySeedSignatureForTesting(
    const std::string& seed_bytes,
    const std::string& base64_seed_signature) {
  return VerifySeedSignature(seed_bytes, base64_seed_signature);
}

VariationsSeedStore::SeedData::SeedData() = default;
VariationsSeedStore::SeedData::~SeedData() = default;
VariationsSeedStore::SeedData::SeedData(VariationsSeedStore::SeedData&& other) =
    default;
VariationsSeedStore::SeedData& VariationsSeedStore::SeedData::operator=(
    VariationsSeedStore::SeedData&& other) = default;

VariationsSeedStore::SeedProcessingResult::SeedProcessingResult(
    SeedData seed_data,
    StoreSeedResult result)
    : seed_data(std::move(seed_data)), result(result) {}
VariationsSeedStore::SeedProcessingResult::~SeedProcessingResult() = default;
VariationsSeedStore::SeedProcessingResult::SeedProcessingResult(
    VariationsSeedStore::SeedProcessingResult&& other) = default;
VariationsSeedStore::SeedProcessingResult&
VariationsSeedStore::SeedProcessingResult::operator=(
    VariationsSeedStore::SeedProcessingResult&& other) = default;

// It is intentional that country-related prefs are retained for regular seeds
// and cleared for safe seeds.
//
// For regular seeds, the prefs are kept for two reasons. First, it's better to
// have some idea of a country recently associated with the device. Second, some
// past, country-gated launches started relying on the VariationsService-
// provided country when they retired server-side configs.
//
// The safe seed prefs are needed to correctly apply a safe seed, so if the safe
// seed is cleared, there's no reason to retain them as they may be incorrect
// for the next safe seed.
void VariationsSeedStore::ClearPrefs(SeedType seed_type) {
  if (seed_type == SeedType::LATEST) {
    local_state_->ClearPref(prefs::kVariationsSeedSerialNumber);
    // Seed and other related information is cleared by the SeedReaderWriter.
    seed_reader_writer_->ClearSeedInfo();
    return;
  }

  DCHECK_EQ(seed_type, SeedType::SAFE);
  safe_seed_store_->ClearState();
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void VariationsSeedStore::ImportInitialSeed(
    std::unique_ptr<SeedResponse> initial_seed) {
  if (initial_seed->data.empty()) {
    // Note: This is an expected case on non-first run starts.
    RecordFirstRunSeedImportResult(
        FirstRunSeedImportResult::kFailNoFirstRunSeed);
    return;
  }

  if (initial_seed->date.is_null()) {
    RecordFirstRunSeedImportResult(
        FirstRunSeedImportResult::kFailInvalidResponseDate);
    LOG(WARNING) << "Missing response date";
    return;
  }

  auto done_callback =
      base::BindOnce([](bool store_success, VariationsSeed seed) {
        if (store_success) {
          RecordFirstRunSeedImportResult(FirstRunSeedImportResult::kSuccess);
        } else {
          RecordFirstRunSeedImportResult(
              FirstRunSeedImportResult::kFailStoreFailed);
          LOG(WARNING) << "First run variations seed is invalid.";
        }
      });
  StoreSeedData(std::move(done_callback), std::move(initial_seed->data),
                std::move(initial_seed->signature),
                std::move(initial_seed->country), initial_seed->date,
                /*is_delta_compressed=*/false, initial_seed->is_gzip_compressed,
                /*require_synchronous=*/true);
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// static
std::optional<std::string> VariationsSeedStore::SeedBytesToCompressedBase64Seed(
    const std::string& seed_bytes) {
  if (seed_bytes.empty()) {
    return std::nullopt;
  }

  std::string compressed_seed_data;
  if (!compression::GzipCompress(seed_bytes, &compressed_seed_data)) {
    return std::nullopt;
  }

  return base::Base64Encode(compressed_seed_data);
}

SeedReaderWriter* VariationsSeedStore::GetSeedReaderWriterForTesting() {
  return seed_reader_writer_.get();
}

void VariationsSeedStore::SetSeedReaderWriterForTesting(
    std::unique_ptr<SeedReaderWriter> seed_reader_writer) {
  seed_reader_writer_ = std::move(seed_reader_writer);
}

SeedReaderWriter* VariationsSeedStore::GetSafeSeedReaderWriterForTesting() {
  return safe_seed_store_->GetSeedReaderWriterForTesting();  // IN-TEST
}

void VariationsSeedStore::SetSafeSeedReaderWriterForTesting(
    std::unique_ptr<SeedReaderWriter> seed_reader_writer) {
  safe_seed_store_->SetSeedReaderWriterForTesting(  // IN-TEST
      std::move(seed_reader_writer));
}

void VariationsSeedStore::StoreLatestSerialNumber(
    std::string_view serial_number) {
  local_state_->SetString(prefs::kVariationsSeedSerialNumber, serial_number);
}

LoadSeedResult VariationsSeedStore::VerifyAndParseSeedImpl(
    VariationsSeed* seed,
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    std::optional<VerifySignatureResult>* verify_signature_result) {
  // TODO(crbug.com/40228403): get rid of |signature_verification_enabled_| and
  // only support switches::kAcceptEmptySeedSignatureForTesting.
  if (signature_verification_enabled_ &&
      !AcceptEmptySeedSignatureForTesting(base64_seed_signature)) {
    *verify_signature_result =
        VerifySeedSignature(seed_data, base64_seed_signature);
    if (*verify_signature_result != VerifySignatureResult::kValidSignature) {
      return LoadSeedResult::kInvalidSignature;
    }
  }

  if (!seed->ParseFromString(seed_data)) {
    return LoadSeedResult::kCorruptProtobuf;
  }

  return LoadSeedResult::kSuccess;
}

// TODO: crbug.com/447171999 - Verification and parse of the seed can be done in
// a background thread after startup.
void VariationsSeedStore::VerifyAndParseSeedAndRunCallback(
    LoadSeedCallback done_callback,
    SeedType seed_type,
    SeedReaderWriter::ReadSeedDataResult read_result) {
  if (read_result.result != LoadSeedResult::kSuccess) {
    LogLoadSeedResult(seed_type, read_result.result);
    std::move(done_callback)
        .Run(/*seed_data=*/"",
             /*seed_signature=*/"", /*success=*/false, VariationsSeed());
    return;
  }
  VariationsSeed seed;
  std::optional<VerifySignatureResult> verify_signature_result;
  LoadSeedResult result =
      VerifyAndParseSeedImpl(&seed, read_result.seed_data,
                             read_result.signature, &verify_signature_result);
  if (verify_signature_result.has_value()) {
    VerifySignatureResult signature_result = verify_signature_result.value();
    if (seed_type == SeedType::LATEST) {
      base::UmaHistogramEnumeration("Variations.LoadSeedSignature",
                                    signature_result);
    } else {
      base::UmaHistogramEnumeration(
          "Variations.SafeMode.LoadSafeSeed.SignatureValidity",
          signature_result);
    }
    if (signature_result != VerifySignatureResult::kValidSignature) {
      ClearPrefs(seed_type);
    }
  }
  if (result == LoadSeedResult::kCorruptProtobuf) {
    ClearPrefs(seed_type);
  }
  LogLoadSeedResult(seed_type, result);
  std::move(done_callback)
      .Run(std::move(read_result.seed_data), std::move(read_result.signature),
           /*success=*/result == LoadSeedResult::kSuccess, std::move(seed));
}

void VariationsSeedStore::LogLoadSeedResult(SeedType seed_type,
                                            LoadSeedResult result) {
  if (seed_type == SeedType::LATEST) {
    RecordLoadSeedResult(result);
  } else {
    RecordLoadSafeSeedResult(result);
  }
}

LoadSeedResult VariationsSeedStore::ReadSeedData(
    SeedType seed_type,
    std::string* seed_data,
    std::string* base64_seed_signature) {
  LoadSeedResult load_seed_result =
      seed_type == SeedType::LATEST
          ? seed_reader_writer_->ReadSeedDataOnStartup(seed_data,
                                                       base64_seed_signature)
          : safe_seed_store_->ReadSeedData(seed_data, base64_seed_signature);
  if (load_seed_result != LoadSeedResult::kSuccess) {
    ClearPrefs(seed_type);
    return load_seed_result;
  }
  // As a space optimization, the latest seed might not be stored directly, but
  // rather aliased to the safe seed.
  if (seed_type == SeedType::LATEST &&
      seed_reader_writer_->IsIdenticalToSafeSeedSentinel()) {
    return ReadSeedData(SeedType::SAFE, seed_data, base64_seed_signature);
  }
  return LoadSeedResult::kSuccess;
}

void VariationsSeedStore::ReadSeedData(
    SeedReaderWriter::ReadSeedDataCallback done_callback,
    SeedType seed_type,
    bool require_synchronous) {
  auto cb = base::BindOnce(
      &VariationsSeedStore::CheckReadSeedDataResultAndRunCallback,
      weak_ptr_factory_.GetWeakPtr(), std::move(done_callback), seed_type,
      require_synchronous);
  if (require_synchronous) {
    std::string seed_data;
    std::string base64_seed_signature;
    auto result = ReadSeedData(seed_type, &seed_data, &base64_seed_signature);
    std::move(cb).Run(SeedReaderWriter::ReadSeedDataResult{
        result, std::move(seed_data), std::move(base64_seed_signature)});
    return;
  }
  if (seed_type == SeedType::LATEST &&
      !seed_reader_writer_->IsIdenticalToSafeSeedSentinel()) {
    seed_reader_writer_->ReadSeedData(std::move(cb));
  } else {
    safe_seed_store_->ReadSeedData(std::move(cb));
  }
}

void VariationsSeedStore::CheckReadSeedDataResultAndRunCallback(
    SeedReaderWriter::ReadSeedDataCallback done_callback,
    SeedType seed_type,
    bool require_synchronous,
    SeedReaderWriter::ReadSeedDataResult load_seed_result) {
  if (load_seed_result.result != LoadSeedResult::kSuccess) {
    ClearPrefs(seed_type);
  }
  std::move(done_callback).Run(std::move(load_seed_result));
}

void VariationsSeedStore::OnSeedDataProcessed(
    base::OnceCallback<void(bool, VariationsSeed)> done_callback,
    bool require_synchronous,
    SeedProcessingResult result) {
  if (result.result != StoreSeedResult::kSuccess) {
    RecordStoreSeedResult(result.result);
    std::move(done_callback).Run(false, VariationsSeed());
    return;
  }

  if (result.validate_result != StoreSeedResult::kSuccess) {
    RecordStoreSeedResult(result.validate_result);
    if (result.seed_data.is_delta_compressed) {
      RecordStoreSeedResult(StoreSeedResult::kFailedDeltaStore);
    }
    std::move(done_callback).Run(false, VariationsSeed());
    return;
  }

  SeedReaderWriter::ReadSeedDataCallback store_validated_seed_cb =
      base::BindOnce(&VariationsSeedStore::StoreValidatedSeed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done_callback),
                     std::move(result.validated), result.seed_data.country_code,
                     result.seed_data.date_fetched, require_synchronous);
  ReadSeedData(/*done_callback=*/std::move(store_validated_seed_cb),
               SeedType::SAFE, require_synchronous);
}

void VariationsSeedStore::ProcessAndStoreSeedData(
    base::OnceCallback<void(bool, VariationsSeed)> done_callback,
    SeedData seed_data,
    bool require_synchronous,
    SeedReaderWriter::ReadSeedDataResult read_result) {
  if (read_result.result != LoadSeedResult::kSuccess) {
    RecordStoreSeedResult(StoreSeedResult::kFailedDeltaReadSeed);
    std::move(done_callback).Run(false, VariationsSeed());
    return;
  }
  seed_data.existing_seed_bytes = std::move(read_result.seed_data);
  if (require_synchronous) {
    SeedProcessingResult result =
        ProcessSeedData(signature_verification_enabled_, std::move(seed_data));
    OnSeedDataProcessed(std::move(done_callback), require_synchronous,
                        std::move(result));
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&VariationsSeedStore::ProcessSeedData,
                       signature_verification_enabled_, std::move(seed_data)),
        base::BindOnce(&VariationsSeedStore::OnSeedDataProcessed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(done_callback),
                       require_synchronous));
  }
}

void VariationsSeedStore::StoreValidatedSeed(
    base::OnceCallback<void(bool, VariationsSeed)> done_callback,
    ValidatedSeed seed,
    std::string country_code,
    base::Time date_fetched,
    bool require_synchronous,
    SeedReaderWriter::ReadSeedDataResult safe_seed_read_result) {
#if BUILDFLAG(IS_ANDROID)
  // If currently we do not have any stored seed, then we mark seed storing as
  // successful on the Java side to avoid repeated seed fetches.
  if (use_first_run_prefs_) {
    ReadSeedData(/*done_callback=*/base::BindOnce(
                     &MarkVariationsSeedAsStoredIfEmptySeed),
                 SeedType::LATEST, require_synchronous);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  int milestone = version_info::GetMajorVersionNumberAsInt();

  LogSeedDayChange(date_fetched);

  // As a space optimization, store an alias to the safe seed if the contents
  // are identical.
  std::string_view seed_data;
  if (safe_seed_read_result.result == LoadSeedResult::kSuccess &&
      safe_seed_read_result.seed_data == seed.seed_data) {
    seed_data = kIdenticalToSafeSeedSentinel;
  } else {
    seed_data = seed.seed_data;
  }
  StoreSeedResult result =
      seed_reader_writer_->StoreValidatedSeedInfo(ValidatedSeedInfo{
          .seed_data = seed_data,
          .signature = seed.base64_seed_signature,
          .milestone = milestone,
          .seed_date = date_fetched,
          .client_fetch_time = base::Time::Now(),
          .session_country_code = country_code,
      });
  if (result == StoreSeedResult::kSuccess) {
    StoreLatestSerialNumber(seed.parsed.serial_number());
  }
  RecordStoreSeedResult(result);
  std::move(done_callback).Run(true, std::move(seed.parsed));
}

void VariationsSeedStore::StoreValidatedSafeSeed(
    base::OnceCallback<void(StoreSeedResult)> done_callback,
    ValidatedSeed seed,
    int seed_milestone,
    base::Time reference_date,
    std::string session_consistency_country,
    std::string permanent_consistency_country,
    std::string locale,
    base::Time seed_fetch_time,
    SeedReaderWriter::ReadSeedDataResult safe_seed_read_result,
    SeedReaderWriter::ReadSeedDataResult latest_seed_read_result) {
  // Before updating the safe seed, update the latest seed if the latest
  // seed's value is |kIdenticalToSafeSeedSentinel|.
  //
  // It's theoretically possible for the client to be in the following state:
  // 1. The client has safe seed A.
  // 2. The client is applying seed B. In other words, seed B was the latest
  //    seed when Chrome was started.
  // 3. The client has just successfully fetched a new latest seed that
  //    happens to be seed A—perhaps due to a rollback. In this case,
  //    |kIdenticalToSafeSeedSentinel| is stored as the latest seed value to
  //    avoid duplicating seed A in storage.
  // 4. The client is promoting seed B to safe seed.
  const SeedInfo latest_seed_info = seed_reader_writer_->GetSeedInfo();
  if (safe_seed_read_result.result == LoadSeedResult::kSuccess &&
      safe_seed_read_result.seed_data != seed.seed_data &&
      seed_reader_writer_->IsIdenticalToSafeSeedSentinel()) {
    StoreSeedResult store_result =
        seed_reader_writer_->StoreValidatedSeedInfo(ValidatedSeedInfo{
            .seed_data = safe_seed_read_result.seed_data,
            .signature = latest_seed_info.signature,
            .milestone = latest_seed_info.milestone,
            .seed_date = latest_seed_info.seed_date,
            .client_fetch_time = latest_seed_info.client_fetch_time,
        });
    if (store_result != StoreSeedResult::kSuccess) {
      std::move(done_callback).Run(store_result);
      return;
    }
  }

  {
    StoreSeedResult store_result =
        safe_seed_store_->SetCompressedSeed(ValidatedSeedInfo{
            .seed_data = seed.seed_data,
            .signature = seed.base64_seed_signature,
            .milestone = seed_milestone,
            .seed_date = reference_date,
            .client_fetch_time = seed_fetch_time,
            .session_country_code = session_consistency_country,
            .permanent_country_code = permanent_consistency_country,
            // The permanent version is not stored in the safe seed, only the
            // country.
            .permanent_country_version = "",
        });
    if (store_result != StoreSeedResult::kSuccess) {
      std::move(done_callback).Run(store_result);
      return;
    }
  }
  safe_seed_store_->SetLocale(locale);

  // As a space optimization, overwrite the stored latest seed data with an
  // alias to the safe seed, if they are identical.
  if (latest_seed_read_result.result == LoadSeedResult::kSuccess &&
      latest_seed_read_result.seed_data == seed.seed_data) {
    StoreSeedResult store_result =
        seed_reader_writer_->StoreValidatedSeedInfo(ValidatedSeedInfo{
            .seed_data = kIdenticalToSafeSeedSentinel,
            .signature = latest_seed_info.signature,
            .milestone = latest_seed_info.milestone,
            .seed_date = latest_seed_info.seed_date,
            .client_fetch_time = latest_seed_info.client_fetch_time,
        });
    if (store_result != StoreSeedResult::kSuccess) {
      std::move(done_callback).Run(store_result);
      return;
    }

    // Moreover, in this case, the last fetch time for the safe seed should
    // match the latest seed's.
    safe_seed_store_->SetFetchTime(latest_seed_info.client_fetch_time);
  }
  std::move(done_callback).Run(StoreSeedResult::kSuccess);
}

// static
VariationsSeedStore::SeedProcessingResult VariationsSeedStore::ProcessSeedData(
    bool signature_verification_enabled,
    SeedData seed_data) {
  const std::string* data = &seed_data.data;

  // If the data is gzip compressed, first uncompress it.
  std::string ungzipped_data;
  if (seed_data.is_gzip_compressed) {
    StoreSeedResult result = Uncompress(*data, &ungzipped_data);
    if (result != StoreSeedResult::kSuccess) {
      return {std::move(seed_data), result};
    }
    data = &ungzipped_data;
  }

  // If the data is delta-compressed, apply the delta patch.
  std::string patched_data;
  if (seed_data.is_delta_compressed) {
    DCHECK(!seed_data.existing_seed_bytes.empty());
    if (!ApplyDeltaPatch(seed_data.existing_seed_bytes, *data, &patched_data)) {
      return {std::move(seed_data), StoreSeedResult::kFailedDeltaApply};
    }
    data = &patched_data;
  }

  ValidatedSeed validated;
  auto validate_result = VariationsSeedStore::ValidateSeedBytes(
      *data, seed_data.base64_seed_signature,
      VariationsSeedStore::SeedType::LATEST, signature_verification_enabled,
      &validated);
  // Important, this must come after the above call as `data` can point to a
  // member of `seed_data` which is being moved.
  SeedProcessingResult result(std::move(seed_data), StoreSeedResult::kSuccess);
  result.validate_result = validate_result;
  result.validated = std::move(validated);
  return result;
}

// static
StoreSeedResult VariationsSeedStore::ValidateSeedBytes(
    const std::string& seed_bytes,
    const std::string& base64_seed_signature,
    SeedType seed_type,
    bool signature_verification_enabled,
    ValidatedSeed* result) {
  DCHECK(result);
  if (seed_bytes.empty()) {
    return StoreSeedResult::kFailedEmptyGzipContents;
  }

  // Only store the seed data if it parses correctly.
  VariationsSeed seed;
  if (!seed.ParseFromString(seed_bytes)) {
    return StoreSeedResult::kFailedParse;
  }

  // TODO(crbug.com/40228403): get rid of |signature_verification_enabled| and
  // only support switches::kAcceptEmptySeedSignatureForTesting.
  if (signature_verification_enabled &&
      !AcceptEmptySeedSignatureForTesting(base64_seed_signature)) {
    const VerifySignatureResult verify_result =
        VerifySeedSignature(seed_bytes, base64_seed_signature);
    switch (seed_type) {
      case SeedType::LATEST:
        base::UmaHistogramEnumeration("Variations.StoreSeedSignature",
                                      verify_result);
        break;
      case SeedType::SAFE:
        base::UmaHistogramEnumeration(
            "Variations.SafeMode.StoreSafeSeed.SignatureValidity",
            verify_result);
        break;
    }

    if (verify_result != VerifySignatureResult::kValidSignature) {
      return StoreSeedResult::kFailedSignature;
    }
  }

  result->seed_data = seed_bytes;
  result->base64_seed_signature = base64_seed_signature;
  result->parsed.Swap(&seed);
  return StoreSeedResult::kSuccess;
}

// static
bool VariationsSeedStore::ApplyDeltaPatch(const std::string& existing_data,
                                          const std::string& patch,
                                          std::string* output) {
  output->clear();

  google::protobuf::io::CodedInputStream in(
      reinterpret_cast<const uint8_t*>(patch.data()), patch.length());
  // Temporary string declared outside the loop so it can be re-used between
  // different iterations (rather than allocating new ones).
  std::string temp;

  const uint32_t existing_data_size =
      static_cast<uint32_t>(existing_data.size());
  while (in.CurrentPosition() != static_cast<int>(patch.length())) {
    uint32_t value;
    if (!in.ReadVarint32(&value)) {
      return false;
    }

    if (value != 0) {
      // A non-zero value indicates the number of bytes to copy from the patch
      // stream to the output.

      // No need to guard against bad data (i.e. very large |value|) because the
      // call below will fail if |value| is greater than the size of the patch.
      if (!in.ReadString(&temp, value)) {
        return false;
      }
      output->append(temp);
    } else {
      // Otherwise, when it's zero, it indicates that it's followed by a pair of
      // numbers - |offset| and |length| that specify a range of data to copy
      // from |existing_data|.
      uint32_t offset;
      uint32_t length;
      if (!in.ReadVarint32(&offset) || !in.ReadVarint32(&length)) {
        return false;
      }

      // Check for |offset + length| being out of range and for overflow.
      base::CheckedNumeric<uint32_t> end_offset(offset);
      end_offset += length;
      if (!end_offset.IsValid() ||
          end_offset.ValueOrDie() > existing_data_size) {
        return false;
      }
      output->append(existing_data, offset, length);
    }
  }
  return true;
}

void VariationsSeedStore::AllowToPurgeSeedsDataFromMemory() {
  seed_reader_writer_->AllowToPurgeSeedDataFromMemory();
  safe_seed_store_->AllowToPurgeSeedDataFromMemory();
}

void VariationsSeedStore::GetStoredSeedInfoForDebugging(
    base::OnceCallback<void(StoredSeedInfo)> done_callback,
    SeedType seed_type) {
  if (seed_type == SeedType::LATEST) {
    seed_reader_writer_->GetStoredSeedInfoForDebugging(
        std::move(done_callback));
  } else {
    safe_seed_store_->GetStoredSeedInfoForDebugging(std::move(done_callback));
  }
}

}  // namespace variations
