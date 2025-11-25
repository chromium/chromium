// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/seed_reader_writer.h"

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/values_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/version_info/channel.h"
#include "components/prefs/pref_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_features.h"
#include "third_party/zlib/google/compression_utils.h"

// ZSTD is not supported on Android due to binary size increase. See
// crbug.com/40196713.
// Note: When changing this, seed file migration logic will need to be added.
#define USE_ZSTD_FOR_SEEDS !BUILDFLAG(IS_ANDROID)

#if USE_ZSTD_FOR_SEEDS
#include "third_party/zstd/src/lib/zstd.h"  // nogncheck
#endif

namespace variations {
namespace {

// A corrupt seed could result in a very large buffer being allocated which
// could crash the process. The maximum size of an uncompressed seed at 50 MiB.
// This is a very high limit and is only used to protect against extreme cases.
constexpr std::size_t kMaxUncompressedSeedSize = 50 * 1024 * 1024;

// A struct to hold the permanent country code and version. Because they're
// stored in a single pref, we need to read them together.
// TODO(crbug.com/411431524): Remove this once it's stored in the Seed File.
struct PermanentCountryVersion {
  std::string_view country;
  std::string_view version;
};

// Histogram suffix used by ImportantFileWriter for recording seed file write
// information.
constexpr char kSeedWriterHistogramSuffix[] = "VariationsSeedsV2";

#if USE_ZSTD_FOR_SEEDS
// The compression level to use for ZSTD.
// TODO(crbug.com/453558393): A locally run benchmark was performed to determine
// the initial compression level. Run an experiment to determine the optimal
// compression level.
constexpr int kZstdCompressionLevel = 2;
#endif  // USE_ZSTD_FOR_SEEDS

bool Compress(std::string_view uncompressed_data,
              std::string* compressed_data) {
  CHECK(compressed_data) << "compressed_data is null";
#if USE_ZSTD_FOR_SEEDS
  auto buff_size = ZSTD_compressBound(uncompressed_data.size());
  compressed_data->resize(buff_size);
  auto seed_compressed_size = ZSTD_compress(
      /*dst=*/compressed_data->data(), /*dstCapacity=*/buff_size,
      /*src=*/uncompressed_data.data(),
      /*srcSize=*/uncompressed_data.size(), kZstdCompressionLevel);
  return seed_compressed_size > 0;
#else   // !USE_ZSTD_FOR_SEEDS
  // Android does not support ZSTD because of binary size increase, so use gzip
  // compression instead.
  return compression::GzipCompress(uncompressed_data, compressed_data);
#endif  // USE_ZSTD_FOR_SEEDS
}

base::expected<std::string, LoadSeedResult> Uncompress(
    std::string_view compressed_data) {
  std::string uncompressed_contents;
#if USE_ZSTD_FOR_SEEDS
  auto uncompressed_buff_size =
      ZSTD_getFrameContentSize(compressed_data.data(), compressed_data.size());
  if (uncompressed_buff_size == ZSTD_CONTENTSIZE_ERROR ||
      uncompressed_buff_size == ZSTD_CONTENTSIZE_UNKNOWN) {
    return base::unexpected(LoadSeedResult::kZstdContentSizeError);
  }
  if (uncompressed_buff_size > kMaxUncompressedSeedSize) {
    return base::unexpected(LoadSeedResult::kExceedsUncompressedSizeLimit);
  }
  uncompressed_contents.resize(uncompressed_buff_size);

  size_t actual_compressed_size = ZSTD_findFrameCompressedSize(
      compressed_data.data(), compressed_data.size());
  if (ZSTD_isError(actual_compressed_size)) {
    return base::unexpected(LoadSeedResult::kCorruptZstd);
  }

  size_t uncompressed_size = ZSTD_decompress(
      uncompressed_contents.data(), uncompressed_contents.size(),
      compressed_data.data(), actual_compressed_size);
  if (ZSTD_isError(uncompressed_size)) {
    return base::unexpected(LoadSeedResult::kCorruptZstd);
  }
#else   // !USE_ZSTD_FOR_SEEDS
  // Android does not support ZSTD because of binary size increase, so use gzip
  // compression instead. Migrating to ZSTD will require a migration from the
  // current gzip format.
  if (compression::GetUncompressedSize(compressed_data) >
      kMaxUncompressedSeedSize) {
    return base::unexpected(LoadSeedResult::kExceedsUncompressedSizeLimit);
  }
  if (!compression::GzipUncompress(compressed_data, &uncompressed_contents)) {
    return base::unexpected(LoadSeedResult::kCorruptGzip);
  }
#endif  // USE_ZSTD_FOR_SEEDS
  return uncompressed_contents;
}

// Serializes, compresses, and returns the compressed seed info used during
// write to disk. Will be run asynchronously on a background thread.
std::optional<std::string> DoSerialize(StoredSeedInfo seed_info) {
  std::string compressed_seed_info;
  if (!Compress(seed_info.SerializeAsString(), &compressed_seed_info)) {
    return std::nullopt;
  }
  return compressed_seed_info;
}

// Returns the file path used to store a seed. If `seed_file_dir` is empty, an
// empty file path is returned.
base::FilePath GetFilePath(const base::FilePath& seed_file_dir,
                           base::FilePath::StringViewType filename) {
  return seed_file_dir.empty() ? base::FilePath()
                               : seed_file_dir.Append(filename);
}

// Returns true if the client is eligible to participate in the seed file trial.
bool IsEligibleForSeedFileTrial(version_info::Channel channel,
                                const base::FilePath& seed_file_dir,
                                const EntropyProviders* entropy_providers) {
  // Note platforms that should not participate in the experiment will
  // deliberately pass an empty |seed_file_dir| and null |entropy_provider|.
  if (seed_file_dir.empty() || entropy_providers == nullptr) {
    return false;
  }
  return channel == version_info::Channel::CANARY ||
         channel == version_info::Channel::DEV;
}

// Sets up the seed file experiment which only some clients are eligible for
// (see IsEligibleForSeedFileTrial()).
void SetUpSeedFileTrial(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    version_info::Channel channel) {
  // Verify that the field trial has not already been set up. This may be the
  // case if a SeedReaderWriter associated with a safe seed calls this function
  // before one associated with a latest seed or vice versa.
  if (base::FieldTrialList::TrialExists(kSeedFileTrial)) {
    return;
  }

  // Only 1% of clients on stable should participate in the experiment.
  base::FieldTrial::Probability group_probability =
      channel == version_info::Channel::STABLE ? 1 : 50;

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kSeedFileTrial, /*total_probability=*/100, kDefaultGroup,
          entropy_provider, /*randomization_seed=*/1));

  trial->AppendGroup(kControlGroup, group_probability);
  trial->AppendGroup(kSeedFilesGroup, group_probability);
}

// Returns the permanent country code and version. For the safe seed, version
// always will be empty.
PermanentCountryVersion GetPermanentCountryVersion(PrefService* local_state,
                                                   std::string_view pref_name) {
  // TODO(crbug.com/411431524): Remove this once it's stored in the Seed File.
  // We need to check because the safe seed pref is a string while the latest
  // seed pref is a list.
  if (pref_name == prefs::kVariationsSafeSeedPermanentConsistencyCountry) {
    return {.country = local_state->GetString(pref_name), .version = ""};
  }
  const auto& list_value = local_state->GetList(pref_name);
  PermanentCountryVersion result;
  if (list_value.size() == 2) {
    const std::string* stored_version = nullptr;
    // We don't need to check the validity of the version here, as it's done
    // later by
    // VariationsFieldTrialCreatorBase::LoadPermanentConsistencyCountry().
    if ((stored_version = list_value[0].GetIfString())) {
      result.version = *stored_version;
    }
    const std::string* stored_country = nullptr;
    if ((stored_country = list_value[1].GetIfString())) {
      result.country = *stored_country;
    }
  }
  return result;
}

// Stores the permanent country code and version in local state. For the safe
// seed, the version is always empty.
void SetPermanentCountryVersion(PrefService* local_state,
                                std::string_view pref_name,
                                std::string_view country_code,
                                std::string_view version) {
  // TODO(crbug.com/411431524): Remove this once it's stored in the Seed File.
  // We need to check because the safe seed pref is a string while the latest
  // seed pref is a list.
  const bool is_safe_seed =
      pref_name == prefs::kVariationsSafeSeedPermanentConsistencyCountry;
  if (is_safe_seed) {
    local_state->SetString(pref_name, country_code);
  } else {
    base::Value::List list_value;
    list_value.Append(version);
    list_value.Append(country_code);
    local_state->SetList(pref_name, std::move(list_value));
  }
}

int64_t TimeToProtoTime(base::Time time) {
  return time.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

bool ShouldStoreWithoutProcessing(std::string_view seed_data) {
  return seed_data.empty() || seed_data == kIdenticalToSafeSeedSentinel;
}

base::expected<StoredSeedInfo, LoadSeedResult> ReadSeedInfoFromFile(
    base::FilePath file_path) {
  std::string seed_file_data;
  if (!base::ReadFileToString(file_path, &seed_file_data)) {
    return base::unexpected(LoadSeedResult::kErrorReadingFile);
  }
  if (seed_file_data.empty()) {
    return base::unexpected(LoadSeedResult::kEmpty);
  }
  auto uncompress_result = Uncompress(seed_file_data);
  if (!uncompress_result.has_value()) {
    return base::unexpected(uncompress_result.error());
  }
  StoredSeedInfo parsed_seed_info;
  if (!parsed_seed_info.ParseFromString(uncompress_result.value())) {
    return base::unexpected(LoadSeedResult::kSeedInfoParseToProtoError);
  }
  return parsed_seed_info;
}

}  // namespace

const SeedFieldsPrefs kRegularSeedFieldsPrefs = {
    .seed = prefs::kVariationsCompressedSeed,
    .signature = prefs::kVariationsSeedSignature,
    .milestone = prefs::kVariationsSeedMilestone,
    .seed_date = prefs::kVariationsSeedDate,
    .client_fetch_time = prefs::kVariationsLastFetchTime,
    .session_country_code = prefs::kVariationsCountry,
    .permanent_country_code_version =
        prefs::kVariationsPermanentConsistencyCountry,
};

const SeedFieldsPrefs kSafeSeedFieldsPrefs = {
    .seed = prefs::kVariationsSafeCompressedSeed,
    .signature = prefs::kVariationsSafeSeedSignature,
    .milestone = prefs::kVariationsSafeSeedMilestone,
    .seed_date = prefs::kVariationsSafeSeedDate,
    .client_fetch_time = prefs::kVariationsSafeSeedFetchTime,
    .session_country_code = prefs::kVariationsSafeSeedSessionConsistencyCountry,
    .permanent_country_code_version =
        prefs::kVariationsSafeSeedPermanentConsistencyCountry,
};

SeedInfo::SeedInfo(std::string_view signature,
                   int milestone,
                   base::Time seed_date,
                   base::Time client_fetch_time,
                   std::string_view session_country_code,
                   std::string_view permanent_country_code,
                   std::string_view permanent_country_version)
    : signature(signature),
      milestone(milestone),
      seed_date(seed_date),
      client_fetch_time(client_fetch_time),
      session_country_code(session_country_code),
      permanent_country_code(permanent_country_code),
      permanent_country_version(permanent_country_version) {}

SeedInfo::~SeedInfo() = default;

SeedInfo::SeedInfo(const SeedInfo& other) = default;

SeedReaderWriter::SeedReaderWriter(
    PrefService* local_state,
    const base::FilePath& seed_file_dir,
    base::FilePath::StringViewType seed_filename,
    base::FilePath::StringViewType old_seed_filename,
    const SeedFieldsPrefs& fields_prefs,
    version_info::Channel channel,
    const EntropyProviders* entropy_providers,
    std::string_view histogram_suffix,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : local_state_(local_state),
      fields_prefs_(fields_prefs),
      file_task_runner_(std::move(file_task_runner)),
      histogram_suffix_(histogram_suffix) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(local_state_) << "SeedReaderWriter needs a valid local state.";
  // Platforms that are not eligible for the seed file trial will deliberately
  // pass an empty `seed_file_dir`.
  if (!seed_file_dir.empty()) {
    seed_writer_ = std::make_unique<base::ImportantFileWriter>(
        GetFilePath(seed_file_dir, seed_filename), file_task_runner_,
        kSeedWriterHistogramSuffix);
    old_seed_file_path_ = GetFilePath(seed_file_dir, old_seed_filename);
  }
  if (IsEligibleForSeedFileTrial(channel, seed_file_dir, entropy_providers)) {
    SetUpSeedFileTrial(entropy_providers->default_entropy(), channel);
    if (ShouldUseSeedFile()) {
      ReadSeedFile();
    } else if (ShouldMigrateToLocalState(channel)) {
      // Because of the new group assignment, it is possible that a client that
      // previously stored the seed data in the old seed file should now migrate
      // back to local state.
      MigrateToLocalState();
    }
  } else if (ShouldMigrateToLocalState(channel)) {
    // The old seed file experiment was affecting clients in stable and beta
    // channels. Migrate if necessary the seed data from the old seed file to
    // local state.
    MigrateToLocalState();
  }
}

SeedReaderWriter::~SeedReaderWriter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (HasPendingWrite()) {
    seed_writer_->DoScheduledWrite();
  }
}

StoreSeedResult SeedReaderWriter::StoreValidatedSeedInfo(
    ValidatedSeedInfo seed_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    return ScheduleSeedFileWrite(seed_info);
  } else {
    return ScheduleLocalStateWrite(seed_info);
  }
}

void SeedReaderWriter::ClearSeedInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/372009105): Remove if-statements when experiment has ended.
  if (ShouldUseSeedFile()) {
    ScheduleSeedFileClear();
  } else {
    local_state_->ClearPref(fields_prefs_->seed);
    local_state_->ClearPref(fields_prefs_->signature);
    local_state_->ClearPref(fields_prefs_->milestone);
    local_state_->ClearPref(fields_prefs_->seed_date);
    local_state_->ClearPref(fields_prefs_->client_fetch_time);
    // Although only clients in the treatment group write seeds to dedicated
    // seed files, attempt to delete the seed file for clients with
    // Local-State-based seeds. If a client switches experiment groups or
    // channels, their device could have a seed file with stale seed data.
    if (seed_writer_) {
      DeleteSeedFile();
    }
  }
}

void SeedReaderWriter::ClearSessionCountry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    stored_seed_info_.clear_session_country_code();
  }
  local_state_->ClearPref(fields_prefs_->session_country_code);
}

SeedInfo SeedReaderWriter::GetSeedInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    return SeedInfo(
        /*signature=*/stored_seed_info_.signature(),
        /*milestone=*/stored_seed_info_.milestone(),
        /*seed_date=*/ProtoTimeToTime(stored_seed_info_.seed_date()),
        /*client_fetch_time=*/
        ProtoTimeToTime(stored_seed_info_.client_fetch_time()),
        /*session_country_code=*/stored_seed_info_.session_country_code(),
        /*permanent_country_code=*/stored_seed_info_.permanent_country_code(),
        /*permanent_country_version=*/stored_seed_info_.permanent_version());
  } else {
    PermanentCountryVersion permanent_country_version =
        GetPermanentCountryVersion(
            local_state_, fields_prefs_->permanent_country_code_version);
    return SeedInfo(
        /*signature=*/local_state_->GetString(fields_prefs_->signature),
        /*milestone=*/local_state_->GetInteger(fields_prefs_->milestone),
        /*seed_date=*/local_state_->GetTime(fields_prefs_->seed_date),
        /*client_fetch_time=*/
        local_state_->GetTime(fields_prefs_->client_fetch_time),
        /*session_country_code=*/
        local_state_->GetString(fields_prefs_->session_country_code),
        /*permanent_country_code=*/permanent_country_version.country,
        /*permanent_country_version=*/permanent_country_version.version);
  }
}

void SeedReaderWriter::SetTimerForTesting(base::OneShotTimer* timer_override) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (seed_writer_) {
    seed_writer_->SetTimerForTesting(timer_override);  // IN-TEST
  }
}

void SeedReaderWriter::SetSeedDate(base::Time server_date_fetched) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Both groups write the seed date to local state.
  // TODO(crbug.com/380465790): Update seed date in seed files instead of local
  // state if the client is in the treatment group.
  if (ShouldUseSeedFile()) {
    stored_seed_info_.set_seed_date(TimeToProtoTime(server_date_fetched));
  }
  local_state_->SetTime(fields_prefs_->seed_date, server_date_fetched);
}

void SeedReaderWriter::SetFetchTime(base::Time fetch_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Both groups write the fetch time to local state.
  // TODO(crbug.com/380465790): Update fetch time in seed files instead of local
  // state if the client is in the treatment group.
  if (ShouldUseSeedFile()) {
    stored_seed_info_.set_client_fetch_time(TimeToProtoTime(fetch_time));
  }
  local_state_->SetTime(fields_prefs_->client_fetch_time, fetch_time);
}

void SeedReaderWriter::ClearPermanentConsistencyCountryAndVersion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    // TODO(crbug.com/380465790): Clear the values from the seed file if the
    // client is in the treatment group.
    stored_seed_info_.clear_permanent_country_code();
    stored_seed_info_.clear_permanent_version();
  }
  local_state_->ClearPref(fields_prefs_->permanent_country_code_version);
}

void SeedReaderWriter::SetPermanentConsistencyCountryAndVersion(
    const std::string_view country,
    const std::string_view version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    stored_seed_info_.set_permanent_country_code(country);
    stored_seed_info_.set_permanent_version(version);
  }
  SetPermanentCountryVersion(local_state_,
                             fields_prefs_->permanent_country_code_version,
                             country, version);
}

LoadSeedResult SeedReaderWriter::ReadSeedDataOnStartup(
    std::string* seed_data,
    std::string* base64_seed_signature) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // On startup, the seed data should always be kept in memory.
  CHECK(!seed_purgeable_from_memory_)
      << "Seed data should not be purgeable from memory on startup.";
  if (ShouldUseSeedFile()) {
    return ProcessStoredSeedData(
        SeedStorageFormat::kRaw, stored_seed_info_.data(),
        stored_seed_info_.signature(), seed_data, base64_seed_signature);
  } else {
    SeedInfo stored_seed_info = GetSeedInfo();
    return ProcessStoredSeedData(
        SeedStorageFormat::kCompressedAndBase64Encoded,
        /*stored_seed_data=*/local_state_->GetString(fields_prefs_->seed),
        stored_seed_info.signature, seed_data, base64_seed_signature);
  }
}

void SeedReaderWriter::ReadSeedData(
    SeedReaderWriter::ReadSeedDataCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    GetSeedDataFromSeedFile(std::move(done_callback));
  } else {
    GetSeedDataFromLocalState(base::BindOnce(
        &SeedReaderWriter::ProcessStoredSeedDataAndRunCallback,
        weak_ptr_factory_.GetWeakPtr(), std::move(done_callback)));
  }
}

void SeedReaderWriter::StoreRawSeedForTesting(std::string seed_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    seed_writer_->WriteNow(seed_data);
    // Clear the stored seed data in memory so that it is read from the seed
    // file.
    stored_seed_info_.clear_data();
  } else {
    local_state_->SetString(fields_prefs_->seed, std::move(seed_data));
  }
}

void SeedReaderWriter::StoreBase64EncodedSeedAndSignatureForTesting(
    std::string base64_compressed_data,
    std::string base64_signature) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string decoded_seed_data;
  CHECK(base::Base64Decode(base64_compressed_data, &decoded_seed_data))
      << "Failed to decode base64 compressed data";
  std::string uncompressed_seed_data;
  CHECK(compression::GzipUncompress(decoded_seed_data, &uncompressed_seed_data))
      << "Failed to uncompress seed data";
  StoreValidatedSeedInfo(
      ValidatedSeedInfo{.seed_data = std::move(uncompressed_seed_data),
                        .signature = std::move(base64_signature)});
}

bool SeedReaderWriter::IsIdenticalToSafeSeedSentinel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldUseSeedFile()) {
    return stored_seed_info_.data() == kIdenticalToSafeSeedSentinel;
  } else {
    return local_state_->GetString(fields_prefs_->seed) ==
           kIdenticalToSafeSeedSentinel;
  }
}

void SeedReaderWriter::AllowToPurgeSeedDataFromMemory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!seed_purgeable_from_memory_)
      << "AllowToPurgeSeedDataFromMemory() should only be called once.";
  seed_purgeable_from_memory_ = true;
  if (ShouldClearSeedDataFromMemory()) {
    stored_seed_info_.clear_data();
  }
}

void SeedReaderWriter::GetStoredSeedInfoForDebugging(
    base::OnceCallback<void(StoredSeedInfo)> done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The seed info may be stored either on the SeedFile or Local State.
  // TODO(crbug.com/411431524): Load StoredSeedInfo object directly from parsing
  // the SeedFile once all clients are migrated there.
  // In-memory stored fields.
  SeedInfo seed_info = GetSeedInfo();
  StoredSeedInfo stored_seed_info;
  stored_seed_info.set_milestone(seed_info.milestone);
  stored_seed_info.set_seed_date(TimeToProtoTime(seed_info.seed_date));
  stored_seed_info.set_client_fetch_time(
      TimeToProtoTime(seed_info.client_fetch_time));
  stored_seed_info.set_session_country_code(seed_info.session_country_code);
  stored_seed_info.set_permanent_country_code(seed_info.permanent_country_code);
  stored_seed_info.set_permanent_version(seed_info.permanent_country_version);
  auto cb = base::BindOnce(
      [](base::OnceCallback<void(StoredSeedInfo)> done_callback,
         StoredSeedInfo stored_seed_info, ReadSeedDataResult result) {
        stored_seed_info.set_data(result.seed_data);
        stored_seed_info.set_signature(result.signature);
        std::move(done_callback).Run(std::move(stored_seed_info));
      },
      std::move(done_callback), std::move(stored_seed_info));
  ReadSeedData(std::move(cb));
}

// static
std::string SeedReaderWriter::CompressForSeedFileForTesting(
    std::string_view contents) {
  std::string compressed_contents;
  CHECK(Compress(contents, &compressed_contents))
      << "Failed to compress seed data";
  return compressed_contents;
}

// static
bool SeedReaderWriter::UncompressFromSeedFileForTesting(
    std::string_view compressed_contents,
    std::string* uncompressed_contents) {
  auto result = Uncompress(compressed_contents);
  if (result.has_value()) {
    *uncompressed_contents = std::move(result.value());
  }
  return result.has_value();
}

// static
std::size_t SeedReaderWriter::MaxUncompressedSeedSizeForTesting() {
  return kMaxUncompressedSeedSize;
}

// static
base::Time SeedReaderWriter::ProtoTimeToTime(int64_t proto_time) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(proto_time));
}

base::ImportantFileWriter::BackgroundDataProducerCallback
SeedReaderWriter::GetSerializedDataProducerForBackgroundSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // DoSerialize() will be run on a background thread different than the one
  // this function runs on, so `seed_info_.data` is passed as a copy to avoid
  // potential race condition in which the `seed_info_.data is potentially
  // modified at the same time DoSerialize() attempts to access it. We cannot
  // use std::move here as we may attempt to read `seed_info_.data` from memory
  // after a write and before we modify `seed_info_.data` again, in which case
  // unexpected empty data would be read.
  auto call_clear_seed_cb =
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindOnce(&SeedReaderWriter::OnSeedWriteComplete,
                                        weak_ptr_factory_.GetWeakPtr()));
  seed_writer_->RegisterOnNextWriteCallbacks(base::OnceClosure(),
                                             std::move(call_clear_seed_cb));
  // TODO(crbug.com/370539202): Potentially use std::move instead of copy if we
  // are able to move seed data out of memory before the write completes.
  return base::BindOnce(&DoSerialize, stored_seed_info_);
}

bool SeedReaderWriter::ShouldClearSeedDataFromMemory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return seed_purgeable_from_memory_ && !HasPendingWrite() &&
         stored_seed_info_.has_data() && !stored_seed_info_.data().empty() &&
         stored_seed_info_.data() != kIdenticalToSafeSeedSentinel;
}

void SeedReaderWriter::OnSeedWriteComplete(bool write_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ShouldClearSeedDataFromMemory()) {
    stored_seed_info_.clear_data();
  }
}

StoreSeedResult SeedReaderWriter::ScheduleSeedFileWrite(
    ValidatedSeedInfo seed_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set `stored_seed_info_` and `stored_seed_data_`, this will be used later by
  // the background serialization and can be changed multiple times before a
  // scheduled write completes, in which case the background serializer will use
  // the set values at the last call of this function.
  stored_seed_info_.set_data(seed_info.seed_data);
  stored_seed_info_.set_signature(seed_info.signature);
  stored_seed_info_.set_milestone(seed_info.milestone);
  stored_seed_info_.set_seed_date(TimeToProtoTime(seed_info.seed_date));
  stored_seed_info_.set_client_fetch_time(
      TimeToProtoTime(seed_info.client_fetch_time));
  // Only update the latest country code if it is not empty.
  if (!seed_info.session_country_code.empty()) {
    stored_seed_info_.set_session_country_code(seed_info.session_country_code);
  }
  if (!seed_info.permanent_country_code.empty()) {
    stored_seed_info_.set_permanent_country_code(
        seed_info.permanent_country_code);
  }
  if (!seed_info.permanent_country_version.empty()) {
    stored_seed_info_.set_permanent_version(
        seed_info.permanent_country_version);
  }
  // `seed_writer_` will eventually call
  // GetSerializedDataProducerForBackgroundSequence() on *this* object to get
  // a callback that will be run asynchronously. This callback will be used to
  // call the DoSerialize() function which will return the seed data to write
  // to the file. This write will also be asynchronous and on a different
  // thread. Note that it is okay to call this while a write is already
  // occurring in a background thread and that this will result in a new write
  // being scheduled.
  seed_writer_->ScheduleWriteWithBackgroundDataSerializer(this);
  // We still need to update the session country code in local state as it is
  // used by hash_realtime_utils::GetHashRealTimeSelectionConfiguringPrefs().
  if (!seed_info.session_country_code.empty()) {
    local_state_->SetString(fields_prefs_->session_country_code,
                            stored_seed_info_.session_country_code());
  }
  return StoreSeedResult::kSuccess;
}

void SeedReaderWriter::ScheduleSeedFileClear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Set `stored_seed_info_`, this will be used later by the background
  // serialization and can be changed multiple times before a scheduled write
  // completes, in which case the background serializer will use the
  // `stored_seed_info_` set at the last call of this function.

  // Set seed data to an empty string so we keep it in memory and don't read it
  // from disk.
  stored_seed_info_.set_data("");
  stored_seed_info_.clear_signature();
  stored_seed_info_.clear_milestone();
  stored_seed_info_.clear_seed_date();
  stored_seed_info_.clear_client_fetch_time();
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

void SeedReaderWriter::DeleteSeedFile() {
  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                seed_writer_->path()));
}

void SeedReaderWriter::DeleteOldSeedFile() {
  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                old_seed_file_path_));
}

void SeedReaderWriter::ReadSeedFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SeedSource seed_source = SeedSource::kNoSource;
  const auto read_seed_info_result = ReadSeedInfoFromFile(seed_writer_->path());
  base::UmaHistogramEnumeration(
      base::StrCat({"Variations.SeedFileReadResult.", histogram_suffix_}),
      read_seed_info_result.error_or(LoadSeedResult::kSuccess));
  if (read_seed_info_result.has_value()) {
    stored_seed_info_ = std::move(read_seed_info_result.value());
    // Record that the seed file was read successfully.
    seed_source = SeedSource::kSeedFile;
  } else if (read_seed_info_result.error() !=
             LoadSeedResult::kErrorReadingFile) {
    // Check if the read failed because the file was missing. We only want to
    // migrate the seed using the old Seed File or Local State the first time,
    // when the Seed File doesn't exist yet. In posterior runs the file should
    // exist. If there's an error for any other reason, we don't want to
    // fallback, so we just initialize the seed data to empty. Note:
    // base::ReadFileToString() doesn't provide info about why the read failed,
    // but this is most probable due to the file not existing.

    // Set seed data to an empty string so we keep it in memory and don't read
    // it from disk.
    stored_seed_info_.set_data("");
  } else if (ReadOldSeedFile()) {
    // Record that the seed file was read successfully.
    seed_source = SeedSource::kOldSeedFile;
  } else {
    // Export seed data from Local State to a seed file in the following cases.
    // 1. Seed file does not exist because this is the first run. For Windows,
    // the first run seed may be stored in Local State, see
    // https://crsrc.org/s?q=file:chrome_feature_list_creator.cc+symbol:SetupInitialPrefs.
    // 2. Seed file does not exist because this is the first time a client is
    // in the seed file experiment's treatment group.
    // 3. Seed file exists and read failed.
    std::string decoded_data;
    std::string uncompressed_data;
    bool decoded_successfully = base::Base64Decode(
        local_state_->GetString(fields_prefs_->seed), &decoded_data);
    // If the seed is empty, compression::GzipUncompress() will return false.
    // However, we still want to write an empty seed to the file.
    if (decoded_successfully &&
        (decoded_data.empty() ||
         compression::GzipUncompress(decoded_data, &uncompressed_data))) {
      PermanentCountryVersion permanent_country_version =
          GetPermanentCountryVersion(
              local_state_, fields_prefs_->permanent_country_code_version);
      ScheduleSeedFileWrite(ValidatedSeedInfo{
          .seed_data = uncompressed_data,
          .signature = local_state_->GetString(fields_prefs_->signature),
          .milestone = local_state_->GetInteger(fields_prefs_->milestone),
          .seed_date = local_state_->GetTime(fields_prefs_->seed_date),
          .client_fetch_time =
              local_state_->GetTime(fields_prefs_->client_fetch_time),
          .session_country_code =
              local_state_->GetString(fields_prefs_->session_country_code),
          .permanent_country_code = permanent_country_version.country,
          .permanent_country_version = permanent_country_version.version,
      });

      if (!decoded_data.empty()) {
        seed_source = SeedSource::kLocalState;
      }
    }
  }

  base::UmaHistogramEnumeration(
      base::StrCat({"Variations.SeedSource.", histogram_suffix_}), seed_source);
  base::UmaHistogramBoolean(
      base::StrCat({"Variations.SeedFileRead.", histogram_suffix_}),
      seed_source == SeedSource::kSeedFile);

  // Clients using a seed file should clear seed from local state and the old
  // seed file, as it will no longer be used.
  local_state_->ClearPref(fields_prefs_->seed);
  DeleteOldSeedFile();
}

bool SeedReaderWriter::ReadOldSeedFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string seed_file_data;
  const bool success =
      base::ReadFileToString(old_seed_file_path_, &seed_file_data);
  if (!success) {
    return false;
  }
  std::string raw_seed_data;
  // The seed will be stored raw in memory, so we need to process it before
  // storing it. If the seed is invalid, we will store an empty seed in memory.
  auto result =
      ProcessStoredSeedData(SeedStorageFormat::kCompressed, seed_file_data,
                            local_state_->GetString(fields_prefs_->signature),
                            &raw_seed_data, /*signature=*/nullptr);
  if (result == LoadSeedResult::kSuccess) {
    stored_seed_info_.set_data(std::move(raw_seed_data));
  } else {
    // Set seed data to an empty string so we keep it in memory and don't read
    // it from disk.
    stored_seed_info_.set_data("");
  }
  stored_seed_info_.set_signature(
      local_state_->GetString(fields_prefs_->signature));
  stored_seed_info_.set_milestone(
      local_state_->GetInteger(fields_prefs_->milestone));
  stored_seed_info_.set_seed_date(
      TimeToProtoTime(local_state_->GetTime(fields_prefs_->seed_date)));
  stored_seed_info_.set_client_fetch_time(
      TimeToProtoTime(local_state_->GetTime(fields_prefs_->client_fetch_time)));
  stored_seed_info_.set_session_country_code(
      local_state_->GetString(fields_prefs_->session_country_code));
  PermanentCountryVersion permanent_country_version =
      GetPermanentCountryVersion(local_state_,
                                 fields_prefs_->permanent_country_code_version);
  stored_seed_info_.set_permanent_country_code(
      permanent_country_version.country);
  stored_seed_info_.set_permanent_version(permanent_country_version.version);

  // Schedule a write to the new seed file for future Chrome sessions.
  seed_writer_->ScheduleWriteWithBackgroundDataSerializer(this);

  return success;
}

StoreSeedResult SeedReaderWriter::ScheduleLocalStateWrite(
    ValidatedSeedInfo seed_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the seed data is empty or it's the sentinel value, store the given
  // string without compressing.
  std::string seed_data;
  if (ShouldStoreWithoutProcessing(seed_info.seed_data)) {
    seed_data = seed_info.seed_data;
  } else {
    std::string compressed_seed_data;
    if (!compression::GzipCompress(seed_info.seed_data,
                                   &compressed_seed_data)) {
      return StoreSeedResult::kFailedGzip;
    }
    seed_data = base::Base64Encode(compressed_seed_data);
  }
  local_state_->SetString(fields_prefs_->seed, seed_data);
  local_state_->SetString(fields_prefs_->signature, seed_info.signature);
  local_state_->SetInteger(fields_prefs_->milestone, seed_info.milestone);
  local_state_->SetTime(fields_prefs_->seed_date, seed_info.seed_date);
  local_state_->SetTime(fields_prefs_->client_fetch_time,
                        seed_info.client_fetch_time);
  if (!seed_info.session_country_code.empty()) {
    local_state_->SetString(fields_prefs_->session_country_code,
                            seed_info.session_country_code);
  }
  // Version could be empty in case of the SafeSeed.
  if (!seed_info.permanent_country_code.empty()) {
    SetPermanentCountryVersion(
        local_state_, fields_prefs_->permanent_country_code_version,
        seed_info.permanent_country_code, seed_info.permanent_country_version);
  }
  return StoreSeedResult::kSuccess;
}

bool SeedReaderWriter::ShouldUseSeedFile() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Use the plain FieldTrialList API here because the trial is registered
  // client-side in VariationsSeedStore SetUpSeedFileTrial().
  return seed_writer_ &&
         base::FieldTrialList::FindFullName(kSeedFileTrial) == kSeedFilesGroup;
}

bool SeedReaderWriter::ShouldMigrateToLocalState(
    version_info::Channel channel) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return seed_writer_ && base::PathExists(old_seed_file_path_);
}

void SeedReaderWriter::MigrateToLocalState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string seed_file_data;
  const bool success =
      base::ReadFileToString(old_seed_file_path_, &seed_file_data);
  if (success && !seed_file_data.empty()) {
    std::string seed_data = seed_file_data == kIdenticalToSafeSeedSentinel
                                ? kIdenticalToSafeSeedSentinel
                                : base::Base64Encode(seed_file_data);
    local_state_->SetString(fields_prefs_->seed, seed_data);
  }
  DeleteOldSeedFile();
}

void SeedReaderWriter::ProcessStoredSeedDataAndRunCallback(
    ReadSeedDataCallback done_callback,
    SeedStorageFormat stored_seed_storage_format,
    std::string_view stored_seed_data,
    std::string_view stored_signature) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string seed_data;
  std::string base64_seed_signature;
  LoadSeedResult result = ProcessStoredSeedData(
      stored_seed_storage_format, stored_seed_data, stored_signature,
      &seed_data, &base64_seed_signature);
  std::move(done_callback)
      .Run(ReadSeedDataResult{result, std::move(seed_data),
                              std::move(base64_seed_signature)});
}

void SeedReaderWriter::GetSeedDataFromLocalState(
    GetSeedDataCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(done_callback)
      .Run(SeedStorageFormat::kCompressedAndBase64Encoded,
           local_state_->GetString(fields_prefs_->seed),
           local_state_->GetString(fields_prefs_->signature));
}

void SeedReaderWriter::GetSeedDataFromSeedFile(
    ReadSeedDataCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stored_seed_info_.has_data()) {
    // If the seed data is stored in memory, we can just run the callback
    // with the stored data. This will copy the data, but can be run on the
    // main thread.
    std::move(done_callback)
        .Run(ReadSeedDataResult{.result = stored_seed_info_.data().empty()
                                              ? LoadSeedResult::kEmpty
                                              : LoadSeedResult::kSuccess,
                                .seed_data = stored_seed_info_.data(),
                                .signature = stored_seed_info_.signature()});
    return;
  }
  // If we're not keeping the seed data in memory, we need to read it from the
  // file. This needs to be done asynchronously.
  auto read_file_cb = base::BindOnce(
      [](ReadSeedDataCallback done_callback, const std::string histogram_suffix,
         base::expected<StoredSeedInfo, LoadSeedResult> result) {
        base::UmaHistogramEnumeration(
            base::StrCat({"Variations.SeedFileReadResult.", histogram_suffix}),
            result.error_or(LoadSeedResult::kSuccess));
        if (!result.has_value()) {
          std::move(done_callback)
              .Run(ReadSeedDataResult{
                  .result = result.error(), .seed_data = "", .signature = ""});
          return;
        }
        std::move(done_callback)
            .Run(ReadSeedDataResult{
                .result = LoadSeedResult::kSuccess,
                .seed_data = std::move(result->data()),
                .signature = std::move(result->signature())});
      },
      std::move(done_callback), histogram_suffix_);
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadSeedInfoFromFile, seed_writer_->path()),
      std::move(read_file_cb));
}

bool SeedReaderWriter::HasPendingWrite() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return seed_writer_ && seed_writer_->HasPendingWrite();
}

// TODO(crbug.com/433877973): Execute in background thread if sync is not
// required.
// static
LoadSeedResult SeedReaderWriter::ProcessStoredSeedData(
    SeedStorageFormat storage_format,
    std::string_view stored_seed_data,
    std::string_view stored_seed_signature,
    std::string* seed_data,
    std::string* signature) {
  if (stored_seed_data.empty()) {
    return LoadSeedResult::kEmpty;
  }

  // As a space optimization, the latest seed might not be stored directly, but
  // rather aliased to the safe seed. We don't need to store the signature,
  // since it is the same as the safe seed.
  if (stored_seed_data == kIdenticalToSafeSeedSentinel) {
    *seed_data = stored_seed_data;
    return LoadSeedResult::kSuccess;
  }

  std::string_view compressed_data;
  std::string decoded_data;
  switch (storage_format) {
    case SeedStorageFormat::kCompressed:
      compressed_data = stored_seed_data;
      break;
    // Because clients not using a seed file get seed data from local state
    // instead, they need to decode the base64-encoded seed data first.
    case SeedStorageFormat::kCompressedAndBase64Encoded:
      if (!base::Base64Decode(stored_seed_data, &decoded_data)) {
        return LoadSeedResult::kCorruptBase64;
      }
      compressed_data = decoded_data;
      break;
    case SeedStorageFormat::kRaw:
      break;
  }

  if (storage_format == SeedStorageFormat::kRaw) {
    *seed_data = std::move(stored_seed_data);
  } else {
    if (compression::GetUncompressedSize(compressed_data) >
        kMaxUncompressedSeedSize) {
      return LoadSeedResult::kExceedsUncompressedSizeLimit;
    }
    if (!compression::GzipUncompress(compressed_data, seed_data)) {
      return LoadSeedResult::kCorruptGzip;
    }
  }

  if (signature) {
    *signature = stored_seed_signature;
  }

  return LoadSeedResult::kSuccess;
}

}  // namespace variations
