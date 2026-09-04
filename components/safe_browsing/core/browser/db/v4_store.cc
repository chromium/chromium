// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v4_store.h"

#include <algorithm>
#include <array>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/types/to_address.h"
#include "components/crx_file/id_util.h"
#include "components/safe_browsing/core/browser/db/prefix_iterator.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/sb_store_file_format.h"
#include "components/safe_browsing/core/browser/db/v4_rice.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/v5_store.pb.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"
#include "crypto/hash.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

using base::TimeTicks;

namespace safe_browsing {

namespace {

// UMA related strings.
// Part 1: Represent the overall operation being performed.
const char kProcessFullUpdate[] = "SafeBrowsing.V4ProcessFullUpdate";
const char kProcessPartialUpdate[] = "SafeBrowsing.V4ProcessPartialUpdate";
const char kReadFromDisk[] = "SafeBrowsing.V4ReadFromDisk";
// Part 2: Represent the sub-operation being performed as part of the larger
// operation from part 1.
const char kApplyUpdate[] = ".ApplyUpdate";
const char kDecodeAdditions[] = ".DecodeAdditions";
const char kDecodeRemovals[] = ".DecodeRemovals";
const char kAdditionsHashesCountPartialUpdate[] = ".AdditionsHashesCount";
const char kAdditionsHashesCountFullUpdate[] = ".AdditionsHashesCount2";
const char kRemovalsHashesCount[] = ".RemovalsHashesCount";
const char kApplyUpdateDuration[] = ".ApplyUpdateDuration";
const char kVerifyChecksumDuration[] = ".VerifyChecksumDuration";
// Part 3: Represent the unit of value being measured and logged.
const char kResult[] = ".Result";

// The maximum size of additions hashes in a single update response.
const int32_t ADDITIONS_HASHES_COUNT_PARTIAL_UPDATE_MAX = 10000;
const int32_t ADDITIONS_HASHES_COUNT_FULL_UPDATE_MAX = 5000000;

// The maximum size of removals hashes in a single update response.
const int32_t REMOVALS_HASHES_COUNT_MAX = 10000;

void RecordEnumWithAndWithoutSuffix(const std::string& metric,
                                    int32_t value,
                                    int32_t maximum,
                                    const base::FilePath& file_path) {
  base::UmaHistogramExactLinear(metric + kResult, value, maximum);
  std::string suffix = GetUmaSuffixForStore(file_path);
  base::UmaHistogramExactLinear(metric + kResult + suffix, value, maximum);
}

void RecordCountWithAndWithoutSuffix(const std::string& metric,
                                     int32_t value,
                                     int32_t maximum,
                                     const base::FilePath& file_path) {
  base::UmaHistogramCustomCounts(metric, value, /*min=*/1, maximum,
                                 /*buckets=*/50);
  std::string suffix = GetUmaSuffixForStore(file_path);
  base::UmaHistogramCustomCounts(metric + suffix, value, /*min=*/1, maximum,
                                 /*buckets=*/50);
}

void RecordApplyUpdateResult(const std::string& base_metric,
                             ApplyUpdateResult result,
                             const base::FilePath& file_path) {
  RecordEnumWithAndWithoutSuffix(base_metric + kApplyUpdate, result,
                                 APPLY_UPDATE_RESULT_MAX, file_path);
}

void RecordUpdateCategory(SafeBrowsingUpdateCategory category,
                          const base::FilePath& file_path) {
  std::string suffix = GetUmaSuffixForStore(file_path);
  base::UmaHistogramEnumeration("SafeBrowsing.V4Update.Category", category);
  base::UmaHistogramEnumeration("SafeBrowsing.V4Update.Category" + suffix,
                                category);

  base::UmaHistogramEnumeration("SafeBrowsing.SBUpdate.Category", category);
  base::UmaHistogramEnumeration("SafeBrowsing.SBUpdate.Category" + suffix,
                                category);
}

void RecordAdditionsHashesCount(const std::string& base_metric,
                                int32_t count,
                                const base::FilePath& file_path) {
  if (base_metric == kProcessFullUpdate) {
    RecordCountWithAndWithoutSuffix(
        base_metric + kAdditionsHashesCountFullUpdate, count,
        ADDITIONS_HASHES_COUNT_FULL_UPDATE_MAX, file_path);
  } else {
    RecordCountWithAndWithoutSuffix(
        base_metric + kAdditionsHashesCountPartialUpdate, count,
        ADDITIONS_HASHES_COUNT_PARTIAL_UPDATE_MAX, file_path);
  }
}

void RecordRemovalsHashesCount(const std::string& base_metric,
                               int32_t count,
                               const base::FilePath& file_path) {
  RecordCountWithAndWithoutSuffix(base_metric + kRemovalsHashesCount, count,
                                  REMOVALS_HASHES_COUNT_MAX, file_path);
}

void RecordApplyUpdateDuration(const std::string& base_metric,
                               base::TimeDelta duration) {
  base::UmaHistogramTimes(base_metric + kApplyUpdateDuration, duration);
}

void RecordVerifyChecksumDuration(const std::string& base_metric,
                                  base::TimeDelta duration) {
  base::UmaHistogramTimes(base_metric + kVerifyChecksumDuration, duration);
}

void RecordVerifyChecksumDuration(base::TimeDelta duration) {
  RecordVerifyChecksumDuration(kReadFromDisk, duration);
  RecordVerifyChecksumDuration("SafeBrowsing.SBReadFromDisk", duration);
}

void RecordStoreReadResult(StoreReadResult result) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4StoreRead.Result", result,
                            STORE_READ_RESULT_MAX);
  base::UmaHistogramBoolean("SafeBrowsing.SBStoreRead.Success",
                            result == READ_SUCCESS);
}

void RecordStoreWriteResult(StoreWriteResult result) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4StoreWrite.Result", result,
                            STORE_WRITE_RESULT_MAX);
}

// Cleans up files that are no longer needed after a successful write. These are
// hash files that may be left behind in the event of a crash or other failure
// which fails to clean up.
void CleanupExtraFiles(const base::FilePath& store_path,
                       const V4StoreFileFormat& file_format) {
  std::set<base::FilePath> paths_in_use{store_path};
  for (const auto& hash_file : file_format.hash_files()) {
    paths_in_use.insert(
        HashPrefixMap::GetPath(store_path, hash_file.extension()));
  }

  // Iterate through all files that start with the store path name. All hash
  // files will be the store path plus an extension.
  base::FileEnumerator e(
      store_path.DirName(), false, base::FileEnumerator::FILES,
      store_path.BaseName().value() + FILE_PATH_LITERAL(".*"));
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    if (paths_in_use.find(name) == paths_in_use.end()) {
      base::DeleteFile(name);
    }
  }
}

void RecordMigrationTime(base::TimeDelta elapsed,
                         const base::FilePath& store_path) {
  std::string suffix = GetUmaSuffixForStore(store_path);
  if (!suffix.empty()) {
    base::UmaHistogramTimes(
        "SafeBrowsing.V4Store.V5ToV4Migration.TimeTaken" + suffix, elapsed);
  }
}

}  // namespace

using ::google::protobuf::RepeatedField;
using ::google::protobuf::RepeatedPtrField;

std::ostream& operator<<(std::ostream& os, const V4Store& store) {
  os << store.DebugString();
  return os;
}

SBStorePtr V4StoreFactory::CreateStore(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::FilePath& base_path,
    const ListInfo& list_info) {
  const base::FilePath store_path = base_path.AppendASCII(list_info.filename());
  return CreateV4Store(db_task_runner, store_path,
                       /*v5_prefix_size=*/list_info.v5_prefix_size().value(),
                       /*is_eligible_for_migration=*/list_info.list_id() !=
                           GetUrlCsdAllowlistId(),
                       /*is_extensions_blocklist=*/list_info.list_id() ==
                           GetChromeExtMalwareId());
}

V4StorePtr V4StoreFactory::CreateV4Store(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& store_path,
    PrefixSize v5_prefix_size,
    bool is_eligible_for_migration,
    bool is_extensions_blocklist) {
  V4StorePtr new_store(
      new V4Store(task_runner, store_path, v5_prefix_size,
                  is_eligible_for_migration, is_extensions_blocklist),
      SBStoreDeleter(task_runner));
  new_store->Initialize();
  return new_store;
}

void V4Store::Initialize() {
  // If a state already exists, don't re-initilize.
  DCHECK(state_.empty());

  StoreReadResult store_read_result = ReadFromDisk();
  has_valid_data_ = (store_read_result == READ_SUCCESS);
  RecordStoreReadResult(store_read_result);
}

std::string V4Store::GetMetricPrefix() const {
  return "SafeBrowsing.V4Store";
}

V4Store::V4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const base::FilePath& store_path,
                 PrefixSize v5_prefix_size,
                 bool is_eligible_for_migration,
                 bool is_extensions_blocklist,
                 const int64_t old_file_size)
    : SBStore(task_runner, store_path, old_file_size),
      hash_prefix_map_(
          std::make_unique<HashPrefixMap>(store_path, task_runner)),
      v5_prefix_size_(v5_prefix_size),
      is_eligible_for_migration_(is_eligible_for_migration),
      is_extensions_blocklist_(is_extensions_blocklist) {
  CHECK_GT(v5_prefix_size_, 0u);
  if (is_extensions_blocklist_) {
    CHECK_EQ(v5_prefix_size_, 16u);
  }
}

V4Store::~V4Store() = default;

// static
void V4Store::RecordDecodeAdditionsResult(const std::string& base_metric,
                                          V4DecodeResult result,
                                          const base::FilePath& file_path) {
  RecordEnumWithAndWithoutSuffix(base_metric + kDecodeAdditions, result,
                                 DECODE_RESULT_MAX, file_path);
}

// static
void V4Store::RecordDecodeRemovalsResult(const std::string& base_metric,
                                         V4DecodeResult result,
                                         const base::FilePath& file_path) {
  RecordEnumWithAndWithoutSuffix(base_metric + kDecodeRemovals, result,
                                 DECODE_RESULT_MAX, file_path);
}

std::string V4Store::DebugString() const {
  std::string state_base64 = base::Base64Encode(state_);

  return base::StringPrintf("path: %" PRFilePath "; state: %s",
                            store_path_.value().c_str(), state_base64.c_str());
}

void V4Store::Reset() {
  expected_checksum_.clear();
  hash_prefix_map_->Clear();
  state_ = "";
}

std::vector<base::FilePath> V4Store::GetPathsInUse() const {
  std::vector<base::FilePath> paths{store_path_};
  for (const auto& path : hash_prefix_map_->GetPaths()) {
    paths.push_back(path);
  }
  return paths;
}

ApplyUpdateResult V4Store::ProcessPartialUpdateAndWriteToDisk(
    const std::string& metric,
    const HashPrefixMapView& hash_prefix_map_old,
    std::unique_ptr<ListUpdateResponse> response) {
  DCHECK(response->has_response_type());
  DCHECK_EQ(ListUpdateResponse::PARTIAL_UPDATE, response->response_type());

  ApplyUpdateResult result = ProcessUpdate(
      metric, hash_prefix_map_old, response, false /* delay_checksum check */);
  if (result == APPLY_UPDATE_SUCCESS) {
    Checksum checksum = response->checksum();
    response.reset();
    RecordStoreWriteResult(WriteToDisk(checksum));
    return hash_prefix_map_->IsValid();
  }
  return result;
}

ApplyUpdateResult V4Store::ProcessFullUpdateAndWriteToDisk(
    const std::string& metric,
    std::unique_ptr<ListUpdateResponse> response) {
  ApplyUpdateResult result =
      ProcessFullUpdate(metric, response, false /* delay_checksum check */);
  if (result == APPLY_UPDATE_SUCCESS) {
    Checksum checksum = response->checksum();
    response.reset();
    RecordStoreWriteResult(WriteToDisk(checksum));
    return hash_prefix_map_->IsValid();
  }
  return result;
}

ApplyUpdateResult V4Store::ProcessFullUpdate(
    const std::string& metric,
    const std::unique_ptr<ListUpdateResponse>& response,
    bool delay_checksum_check) {
  DCHECK(response->has_response_type());
  DCHECK_EQ(ListUpdateResponse::FULL_UPDATE, response->response_type());
  // TODO(vakh): For a full update, we don't need to process the update in
  // lexographical order to store it, but we do need to do that for calculating
  // checksum. It might save some CPU cycles to store the full update as-is and
  // walk the list of hash prefixes in lexographical order only for checksum
  // calculation.
  return ProcessUpdate(metric, HashPrefixMapView(), response,
                       delay_checksum_check);
}

ApplyUpdateResult V4Store::ProcessUpdate(
    const std::string& metric,
    const HashPrefixMapView& hash_prefix_map_old,
    const std::unique_ptr<ListUpdateResponse>& response,
    bool delay_checksum_check) {
  const RepeatedField<int32_t>* raw_removals = nullptr;
  RepeatedField<int32_t> rice_removals;
  size_t removals_size = response->removals_size();
  DCHECK_LE(removals_size, 1u);
  if (removals_size == 1) {
    const ThreatEntrySet& removal = response->removals().Get(0);
    const CompressionType compression_type = removal.compression_type();
    if (compression_type == RAW) {
      raw_removals = &removal.raw_indices().indices();
    } else if (compression_type == RICE) {
      DCHECK(removal.has_rice_indices());

      const RiceDeltaEncoding& rice_indices = removal.rice_indices();
      V4DecodeResult decode_result = V4RiceDecoder::DecodeIntegers(
          rice_indices.first_value(), rice_indices.rice_parameter(),
          rice_indices.num_entries(), rice_indices.encoded_data(),
          &rice_removals);

      RecordDecodeRemovalsResult(metric, decode_result, store_path_);
      if (decode_result != DECODE_SUCCESS) {
        return RICE_DECODING_FAILURE;
      }
      raw_removals = &rice_removals;
    } else {
      return UNEXPECTED_COMPRESSION_TYPE_REMOVALS_FAILURE;
    }
  }
  if (raw_removals) {
    RecordRemovalsHashesCount(metric, raw_removals->size(), store_path_);
  }

  std::unordered_map<PrefixSize, HashPrefixes> hash_prefix_map;
  ApplyUpdateResult apply_update_result = UpdateHashPrefixMapFromAdditions(
      metric, response->additions(), &hash_prefix_map);
  if (apply_update_result != APPLY_UPDATE_SUCCESS) {
    return apply_update_result;
  }

  std::string expected_checksum;
  if (response->has_checksum() && response->checksum().has_sha256()) {
    expected_checksum = response->checksum().sha256();
  }

  if (delay_checksum_check) {
    DCHECK(hash_prefix_map_old.empty());
    DCHECK(!raw_removals);
    // We delay the checksum check at startup to be able to load the DB
    // quickly. In this case, the |hash_prefix_map_old| should be empty, so just
    // copy over the |hash_prefix_map|.
    for (const auto& kv : hash_prefix_map) {
      hash_prefix_map_->Append(kv.first, kv.second);
    }

    // Calculate the checksum asynchronously later and if it doesn't match,
    // reset the store.
    expected_checksum_ = expected_checksum;
  } else {
    HashPrefixMapView hash_prefix_map_view(hash_prefix_map.begin(),
                                           hash_prefix_map.end());
    apply_update_result = MergeUpdate(hash_prefix_map_old, hash_prefix_map_view,
                                      raw_removals, expected_checksum);
    if (apply_update_result != APPLY_UPDATE_SUCCESS) {
      return apply_update_result;
    }
  }

  state_ = response->new_client_state();
  return APPLY_UPDATE_SUCCESS;
}

void V4Store::ApplyUpdate(
    std::unique_ptr<SBUpdateResponse> response,
    const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner,
    UpdatedStoreReadyCallback callback) {
  CHECK(response->v4_response);
  std::unique_ptr<ListUpdateResponse> v4_response =
      std::move(response->v4_response);
  base::ElapsedThreadTimer thread_timer;
  V4StorePtr new_store(new V4Store(task_runner_, store_path_, v5_prefix_size_,
                                   is_eligible_for_migration_,
                                   is_extensions_blocklist_, file_size_),
                       SBStoreDeleter(task_runner_));
  ApplyUpdateResult apply_update_result;
  std::optional<std::string> metric;
  ApplyUpdateType apply_update_type;
  bool is_full_update =
      (v4_response->response_type() == ListUpdateResponse::FULL_UPDATE);
  SafeBrowsingUpdateCategory category;
  if (is_first_update_after_v5_migration_) {
    category = is_full_update
                   ? SafeBrowsingUpdateCategory::kPostMigrationFullUpdate
                   : SafeBrowsingUpdateCategory::kPostMigrationPartialUpdate;
  } else if (is_new_database_) {
    category = is_full_update
                   ? SafeBrowsingUpdateCategory::kNewDatabaseFullUpdate
                   : SafeBrowsingUpdateCategory::kNewDatabasePartialUpdate;
  } else {
    category = is_full_update
                   ? SafeBrowsingUpdateCategory::kRegularFullUpdate
                   : SafeBrowsingUpdateCategory::kRegularPartialUpdate;
  }
  RecordUpdateCategory(category, store_path_);

  is_first_update_after_v5_migration_ = false;
  is_new_database_ = false;

  if (v4_response->response_type() == ListUpdateResponse::PARTIAL_UPDATE) {
    metric = kProcessPartialUpdate;
    apply_update_type = ApplyUpdateType::kPartial;
    apply_update_result = new_store->ProcessPartialUpdateAndWriteToDisk(
        metric.value(), hash_prefix_map_->view(), std::move(v4_response));
  } else if (v4_response->response_type() == ListUpdateResponse::FULL_UPDATE) {
    apply_update_type = ApplyUpdateType::kFull;
    metric = kProcessFullUpdate;
    apply_update_result = new_store->ProcessFullUpdateAndWriteToDisk(
        metric.value(), std::move(v4_response));
  } else {
    apply_update_type = ApplyUpdateType::kInvalid;
    apply_update_result = UNEXPECTED_RESPONSE_TYPE_FAILURE;
  }

  if (apply_update_result == APPLY_UPDATE_SUCCESS) {
    new_store->has_valid_data_ = true;
    new_store->last_apply_update_result_ = apply_update_result;
    new_store->last_apply_update_time_millis_ = base::Time::Now();
    new_store->checks_attempted_ = checks_attempted_;
  } else {
    new_store.reset();
    DLOG(WARNING) << "Failure: ApplyUpdate: reason: " << apply_update_result
                  << "; store: " << *this;
  }

  // Record the state of the update to be shown in the Safe Browsing page.
  last_apply_update_result_ = apply_update_result;

  base::UmaHistogramEnumeration("SafeBrowsing.V4ProcessUpdate.UpdateType",
                                apply_update_type);
  if (metric.has_value()) {
    RecordApplyUpdateResult(metric.value(), apply_update_result, store_path_);
    RecordApplyUpdateDuration(metric.value(), thread_timer.Elapsed());
  }

  // Posting the task should be the last thing to do in this function.
  // Otherwise, the posted task can end up running in parallel. If that
  // happens, the old store will get destoyed and can lead to use-after-free in
  // this function.
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(new_store)));
}

ApplyUpdateResult V4Store::UpdateHashPrefixMapFromAdditions(
    const std::string& metric,
    const RepeatedPtrField<ThreatEntrySet>& additions,
    std::unordered_map<PrefixSize, HashPrefixes>* additions_map) {
  for (const auto& addition : additions) {
    ApplyUpdateResult apply_update_result = APPLY_UPDATE_SUCCESS;
    const CompressionType compression_type = addition.compression_type();
    if (compression_type == RAW) {
      DCHECK(addition.has_raw_hashes());
      DCHECK(addition.raw_hashes().has_raw_hashes());

      apply_update_result =
          AddUnlumpedHashes(addition.raw_hashes().prefix_size(),
                            addition.raw_hashes().raw_hashes(), additions_map);
    } else if (compression_type == RICE) {
      DCHECK(addition.has_rice_hashes());

      const RiceDeltaEncoding& rice_hashes = addition.rice_hashes();
      std::vector<uint32_t> raw_hashes;
      V4DecodeResult decode_result = V4RiceDecoder::DecodePrefixes(
          rice_hashes.first_value(), rice_hashes.rice_parameter(),
          rice_hashes.num_entries(), rice_hashes.encoded_data(), &raw_hashes);
      RecordDecodeAdditionsResult(metric, decode_result, store_path_);
      if (decode_result != DECODE_SUCCESS) {
        return RICE_DECODING_FAILURE;
      } else {
        char* raw_hashes_start = reinterpret_cast<char*>(raw_hashes.data());
        size_t raw_hashes_size = sizeof(uint32_t) * raw_hashes.size();
        RecordAdditionsHashesCount(metric, raw_hashes_size, store_path_);

        // Rice-Golomb encoding is used to send compressed compressed 4-byte
        // hash prefixes. Hash prefixes longer than 4 bytes will not be
        // compressed, and will be served in raw format instead.
        // Source: https://developers.google.com/safe-browsing/v4/compression
        const PrefixSize kPrefixSize = 4;
        apply_update_result = AddUnlumpedHashes(kPrefixSize, raw_hashes_start,
                                                raw_hashes_size, additions_map);
      }
    } else {
      return UNEXPECTED_COMPRESSION_TYPE_ADDITIONS_FAILURE;
    }

    if (apply_update_result != APPLY_UPDATE_SUCCESS) {
      // If there was an error in updating the map, discard the update entirely.
      return apply_update_result;
    }
  }

  return APPLY_UPDATE_SUCCESS;
}

// static
ApplyUpdateResult V4Store::AddUnlumpedHashes(
    PrefixSize prefix_size,
    const std::string& raw_hashes,
    std::unordered_map<PrefixSize, HashPrefixes>* additions_map) {
  return AddUnlumpedHashes(prefix_size, raw_hashes.data(), raw_hashes.size(),
                           additions_map);
}

// static
ApplyUpdateResult V4Store::AddUnlumpedHashes(
    PrefixSize prefix_size,
    const char* raw_hashes_begin,
    const size_t raw_hashes_length,
    std::unordered_map<PrefixSize, HashPrefixes>* additions_map) {
  if (prefix_size < kMinHashPrefixLength) {
    return PREFIX_SIZE_TOO_SMALL_FAILURE;
  }
  if (prefix_size > kMaxHashPrefixLength) {
    return PREFIX_SIZE_TOO_LARGE_FAILURE;
  }
  if (raw_hashes_length % prefix_size != 0) {
    return ADDITIONS_SIZE_UNEXPECTED_FAILURE;
  }

  // TODO(vakh): Figure out a way to avoid the following copy operation.
  (*additions_map)[prefix_size].append(
      HashPrefixesView(raw_hashes_begin, raw_hashes_length));
  return APPLY_UPDATE_SUCCESS;
}

// static
bool V4Store::GetNextSmallestUnmergedPrefix(
    const HashPrefixMapView& hash_prefix_map,
    const IteratorMap& iterator_map,
    HashPrefixStr* smallest_hash_prefix) {
  HashPrefixStr current_hash_prefix;
  bool has_unmerged = false;

  for (const auto& iterator_pair : iterator_map) {
    PrefixSize prefix_size = iterator_pair.first;
    HashPrefixesView::const_iterator start = iterator_pair.second;

    HashPrefixesView hash_prefixes = hash_prefix_map.at(prefix_size);
    PrefixSize distance = std::distance(start, hash_prefixes.end());
    CHECK_EQ(0u, distance % prefix_size);
    if (prefix_size <= distance) {
      current_hash_prefix = HashPrefixStr(start, start + prefix_size);
      if (!has_unmerged || *smallest_hash_prefix > current_hash_prefix) {
        has_unmerged = true;
        smallest_hash_prefix->swap(current_hash_prefix);
      }
    }
  }
  return has_unmerged;
}

// static
void V4Store::InitializeIteratorMap(const HashPrefixMapView& hash_prefix_map,
                                    IteratorMap* iterator_map) {
  for (const auto& map_pair : hash_prefix_map) {
    (*iterator_map)[map_pair.first] = map_pair.second.begin();
  }
}

ApplyUpdateResult V4Store::MergeUpdateFast(
    const HashPrefixMapView& old_prefixes_map,
    const HashPrefixMapView& additions_map,
    const RepeatedField<int32_t>* raw_removals,
    const std::string& expected_checksum) {
  const PrefixSize prefix_size = old_prefixes_map.begin()->first;

  SBStoreUpdateResult result = MergeUpdateLoop(
      prefix_size, base::as_byte_span(old_prefixes_map.begin()->second),
      base::as_byte_span(additions_map.begin()->second), raw_removals,
      expected_checksum, hash_prefix_map_.get());

  switch (result) {
    case SBStoreUpdateResult::kSuccess:
      return APPLY_UPDATE_SUCCESS;
    case SBStoreUpdateResult::kAdditionsHasExistingPrefixFailure:
      return ADDITIONS_HAS_EXISTING_PREFIX_FAILURE;
    case SBStoreUpdateResult::kRemovalsIndexTooLargeFailure:
      return REMOVALS_INDEX_TOO_LARGE_FAILURE;
    case SBStoreUpdateResult::kChecksumMismatchFailure:
      return CHECKSUM_MISMATCH_FAILURE;
  }
}

ApplyUpdateResult V4Store::MergeUpdate(
    const HashPrefixMapView& old_prefixes_map,
    const HashPrefixMapView& additions_map,
    const RepeatedField<int32_t>* raw_removals,
    const std::string& expected_checksum) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(hash_prefix_map_->view().empty());

  if (raw_removals) {
    for (int32_t index : *raw_removals) {
      if (index < 0) {
        return REMOVALS_INDEX_NEGATIVE_FAILURE;
      }
    }
  }

  bool calculate_checksum = !expected_checksum.empty();
  if (calculate_checksum &&
      (expected_checksum.size() != crypto::hash::kSha256Size)) {
    return CHECKSUM_MISMATCH_FAILURE;
  }

  hash_prefix_map_->Clear();

  if (old_prefixes_map.size() == 1 && additions_map.size() == 1 &&
      old_prefixes_map.begin()->first == additions_map.begin()->first) {
    return MergeUpdateFast(old_prefixes_map, additions_map, raw_removals,
                           expected_checksum);
  }

  IteratorMap old_iterator_map;
  HashPrefixStr next_smallest_prefix_old;
  InitializeIteratorMap(old_prefixes_map, &old_iterator_map);
  bool old_has_unmerged = GetNextSmallestUnmergedPrefix(
      old_prefixes_map, old_iterator_map, &next_smallest_prefix_old);

  IteratorMap additions_iterator_map;
  HashPrefixStr next_smallest_prefix_additions;
  InitializeIteratorMap(additions_map, &additions_iterator_map);
  bool additions_has_unmerged = GetNextSmallestUnmergedPrefix(
      additions_map, additions_iterator_map, &next_smallest_prefix_additions);

  // Classical merge sort.
  // The two constructs to merge are maps: old_prefixes_map, additions_map.
  // At least one of the maps still has elements that need to be merged into the
  // new store.

  crypto::hash::Hasher checksum_ctx(crypto::hash::HashKind::kSha256);

  // Keep track of the number of elements picked from the old map. This is used
  // to determine which elements to drop based on the raw_removals. Note that
  // picked is not the same as merged. A picked element isn't merged if its
  // index is on the raw_removals list.
  int total_picked_from_old = 0;
  auto removals_iter =
      raw_removals ? std::make_optional(raw_removals->begin()) : std::nullopt;
  while (old_has_unmerged || additions_has_unmerged) {
    // If the same hash prefix appears in the existing store and the additions
    // list, something is clearly wrong. Discard the update.
    if (old_has_unmerged && additions_has_unmerged &&
        next_smallest_prefix_old == next_smallest_prefix_additions) {
      return ADDITIONS_HAS_EXISTING_PREFIX_FAILURE;
    }

    // Select which map to pick the next hash prefix from to keep the result in
    // lexographically sorted order.
    bool pick_from_old =
        old_has_unmerged &&
        (!additions_has_unmerged ||
         (next_smallest_prefix_old < next_smallest_prefix_additions));

    PrefixSize next_smallest_prefix_size;
    if (pick_from_old) {
      next_smallest_prefix_size = next_smallest_prefix_old.size();

      // Update the iterator map, which means that we have merged one hash
      // prefix of size |next_smallest_prefix_size| from the old store.
      old_iterator_map[next_smallest_prefix_size] += next_smallest_prefix_size;

      if (!raw_removals || *removals_iter == raw_removals->end() ||
          **removals_iter != total_picked_from_old) {
        // Append the smallest hash to the appropriate list.
        hash_prefix_map_->Append(next_smallest_prefix_size,
                                 next_smallest_prefix_old);

        if (calculate_checksum) {
          checksum_ctx.Update(next_smallest_prefix_old);
        }
      } else {
        // Element not added to new map. Move the removals iterator forward.
        (*removals_iter)++;
      }

      total_picked_from_old++;

      // Find the next smallest unmerged element in the old store's map.
      old_has_unmerged = GetNextSmallestUnmergedPrefix(
          old_prefixes_map, old_iterator_map, &next_smallest_prefix_old);
    } else {
      next_smallest_prefix_size = next_smallest_prefix_additions.size();

      // Append the smallest hash to the appropriate list.
      hash_prefix_map_->Append(next_smallest_prefix_size,
                               next_smallest_prefix_additions);

      if (calculate_checksum) {
        checksum_ctx.Update(next_smallest_prefix_additions);
      }

      // Update the iterator map, which means that we have merged one hash
      // prefix of size |next_smallest_prefix_size| from the update.
      additions_iterator_map[next_smallest_prefix_size] +=
          next_smallest_prefix_size;

      // Find the next smallest unmerged element in the additions map.
      additions_has_unmerged =
          GetNextSmallestUnmergedPrefix(additions_map, additions_iterator_map,
                                        &next_smallest_prefix_additions);
    }
  }

  if (raw_removals && *removals_iter != raw_removals->end()) {
    return REMOVALS_INDEX_TOO_LARGE_FAILURE;
  }

  if (calculate_checksum) {
    std::array<uint8_t, crypto::hash::kSha256Size> checksum;
    checksum_ctx.Finish(checksum);
    auto expected = base::as_byte_span(expected_checksum);
    if (expected != checksum) {
#if DCHECK_IS_ON()
      std::string checksum_b64 = base::Base64Encode(checksum);
      std::string expected_b64 = base::Base64Encode(expected);
      DVLOG(1) << "Failure: Checksum mismatch: calculated: " << checksum_b64
               << "; expected: " << expected_b64 << "; store: " << *this;
#endif
      return CHECKSUM_MISMATCH_FAILURE;
    }
  }

  return APPLY_UPDATE_SUCCESS;
}

V5ToV4MigrationResult V4Store::AttemptV5ToV4Migration() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  if (base::PathExists(store_path_)) {
    return V5ToV4MigrationResult::kDiskAlreadyV4;
  }
  base::FilePath v5_store_path =
      store_path_.InsertBeforeExtension(FILE_PATH_LITERAL("_v5"));
  if (!base::PathExists(v5_store_path)) {
    return V5ToV4MigrationResult::kV5StoreNotFound;
  }

  base::ElapsedTimer timer;
  absl::Cleanup log_timer = [this, &timer] {
    RecordMigrationTime(timer.Elapsed(), store_path_);
  };

  if (!is_eligible_for_migration_) {
    bool wipe_succeeded = WipeV5Store(v5_store_path);
    return wipe_succeeded ? V5ToV4MigrationResult::kStoreIneligibleWipeSucceeded
                          : V5ToV4MigrationResult::kStoreIneligibleWipeFailed;
  }
  return MigrateFromV5(v5_store_path);
}

StoreReadResult V4Store::ReadFromDisk() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  V5ToV4MigrationResult migration_result = AttemptV5ToV4Migration();
  base::UmaHistogramEnumeration("SafeBrowsing.V4Store.V5ToV4MigrationResult",
                                migration_result);
  if (migration_result == V5ToV4MigrationResult::kV5StoreNotFound) {
    is_new_database_ = true;
  } else if (migration_result != V5ToV4MigrationResult::kDiskAlreadyV4) {
    is_first_update_after_v5_migration_ = true;
  }

  switch (migration_result) {
    case V5ToV4MigrationResult::kDiskAlreadyV4:
    case V5ToV4MigrationResult::kV5ToV4MigrationSucceeded:
      return ReadFromDiskInternal();
    case V5ToV4MigrationResult::kStoreIneligibleWipeSucceeded:
      return V5_TO_V4_MIGRATION_WIPED_SUCCESSFULLY;
    case V5ToV4MigrationResult::kV5StoreNotFound:
      return FILE_UNREADABLE_FAILURE;
    case V5ToV4MigrationResult::kReadV5Failed:
    case V5ToV4MigrationResult::kPrefixSizeMismatchFailure:
    case V5ToV4MigrationResult::kHashFileMissingFailure:
    case V5ToV4MigrationResult::kRenameHashFileFailure:
    case V5ToV4MigrationResult::kWriteV4FileFailure:
    case V5ToV4MigrationResult::kRenameV4StoreFileFailure:
    case V5ToV4MigrationResult::kProtoSerializationFailure:
    case V5ToV4MigrationResult::kStoreIneligibleWipeFailed:
    case V5ToV4MigrationResult::kExtensionBlocklistMigrationFailed:
      return V5_TO_V4_MIGRATION_FAILURE;
  }
}

StoreReadResult V4Store::ReadFromDiskInternal() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  V4StoreFileFormat file_format;
  int64_t file_size = 0;
  StoreReadResult validation_result =
      ParseAndValidateV4StoreFileFormat(store_path_, file_format, &file_size);
  if (validation_result != READ_SUCCESS) {
    return validation_result;
  }

  ApplyUpdateResult apply_update_result =
      hash_prefix_map_->ReadFromDisk(SBStoreFileFormat(&file_format));
  if (apply_update_result == APPLY_UPDATE_SUCCESS) {
    std::unique_ptr<ListUpdateResponse> response(new ListUpdateResponse);
    response->Swap(file_format.mutable_list_update_response());
    apply_update_result = ProcessFullUpdate(kReadFromDisk, response,
                                            true /* delay_checksum check */);
  }

  RecordApplyUpdateResult(kReadFromDisk, apply_update_result, store_path_);
  last_apply_update_result_ = apply_update_result;
  if (apply_update_result != APPLY_UPDATE_SUCCESS) {
    hash_prefix_map_->Clear();
    return HASH_PREFIX_MAP_GENERATION_FAILURE;
  }

  // Update |file_size_| now because we parsed the file correctly.
  file_size_ = file_size;
  for (const auto& hash_file : file_format.hash_files()) {
    file_size_ += hash_file.file_size();
  }

  return READ_SUCCESS;
}

ConvertExtensionBlocklistV5ToV4Result
V4Store::ConvertExtensionsBlocklistFromV5ToV4(
    const base::FilePath& v5_hash_file_path,
    const base::FilePath& v4_hash_file_path,
    std::string* checksum_sha256,
    uint64_t* file_size) {
  std::string v5_data;
  if (!base::ReadFileToString(v5_hash_file_path, &v5_data)) {
    return ConvertExtensionBlocklistV5ToV4Result::kReadV5Failed;
  }

  // Verify V5 checksum if provided and not empty. For the purposes of the disk
  // migration, we don't fail when the checksum is missing, because
  // `V4Store::VerifyChecksum` allows it. `V5Store::VerifyChecksum` may end up
  // being different.
  if (checksum_sha256 && !checksum_sha256->empty()) {
    auto calculated_checksum = crypto::hash::Sha256(v5_data);
    if (checksum_sha256->size() != crypto::hash::kSha256Size ||
        base::as_byte_span(*checksum_sha256) != calculated_checksum) {
      return ConvertExtensionBlocklistV5ToV4Result::kV5ChecksumMismatch;
    }
  }
  if (v5_data.size() % 16 != 0) {
    return ConvertExtensionBlocklistV5ToV4Result::kInvalidFileSize;
  }
  base::span<const uint8_t> v5_span = base::as_byte_span(v5_data);
  std::string v4_id_data;
  v4_id_data.reserve(v5_span.size() * 2);
  for (size_t i = 0; i < v5_span.size(); i += 16u) {
    v4_id_data.append(
        crx_file::id_util::GenerateIdFromHash(v5_span.subspan(i, 16u)));
  }
  if (!base::WriteFile(v4_hash_file_path, v4_id_data)) {
    return ConvertExtensionBlocklistV5ToV4Result::kWriteV4Failed;
  }
  *file_size = v4_id_data.size();

  if (checksum_sha256 && !checksum_sha256->empty()) {
    std::array<uint8_t, crypto::hash::kSha256Size> v4_checksum;
    crypto::hash::Hash(crypto::hash::HashKind::kSha256,
                       base::as_byte_span(v4_id_data), v4_checksum);
    checksum_sha256->assign(v4_checksum.begin(), v4_checksum.end());
  }

  return ConvertExtensionBlocklistV5ToV4Result::kSuccess;
}

V5ToV4MigrationResult V4Store::MigrateFromV5(
    const base::FilePath& v5_store_path) {
  const PrefixSize kV4ExtensionIdPrefixSize = 32;
  V5StoreFileFormat v5_file_format;
  int64_t v5_file_size = 0;
  base::FilePath v5_hash_file_path;
  base::FilePath v4_hash_file_path;
  base::FilePath temp_store_path = store_path_.AddExtensionASCII("tmp");
  bool migration_succeeded = false;

  // If we fail to migrate from v5, we wipe the v5 files and the attempted v4
  // file format temp file.
  absl::Cleanup cleanup_on_failure = [&v5_store_path, &v5_hash_file_path,
                                      &v4_hash_file_path, &temp_store_path,
                                      &migration_succeeded] {
    if (!migration_succeeded) {
      base::DeleteFile(v5_store_path);
      if (!v5_hash_file_path.empty()) {
        base::DeleteFile(v5_hash_file_path);
      }
      if (!v4_hash_file_path.empty()) {
        base::DeleteFile(v4_hash_file_path);
      }
      base::DeleteFile(temp_store_path);
    }
  };

  // Parse and validate the existing V5 store file.
  V5StoreReadResult validation_result = ParseAndValidateV5StoreFileFormat(
      v5_store_path, v5_file_format, &v5_file_size);
  if (validation_result != V5StoreReadResult::kReadSuccess) {
    base::UmaHistogramEnumeration(
        "SafeBrowsing.V4Store.V5ToV4Migration.V5ReadFailureReason",
        validation_result);
    return V5ToV4MigrationResult::kReadV5Failed;
  }

  auto* list_details = v5_file_format.mutable_list_details();
  std::string v4_ext;
  PrefixSize v4_prefix_size =
      is_extensions_blocklist_ ? kV4ExtensionIdPrefixSize : v5_prefix_size_;
  uint64_t file_size = 0;
  std::string* v4_checksum = nullptr;
  if (list_details->has_checksum() && list_details->checksum().has_sha256()) {
    v4_checksum = list_details->mutable_checksum()->mutable_sha256();
  }

  // Move the V5 hash file to the V4 path if it exists.
  if (list_details->has_hash_file()) {
    const auto& hash_file = list_details->hash_file();
    // This is used in `cleanup_on_failure` above so it needs to run as soon as
    // we have the `hash_file.extension()` available.
    v5_hash_file_path =
        HashPrefixContainer::GetPath(v5_store_path, hash_file.extension());

    if (hash_file.file_size() % v5_prefix_size_ != 0) {
      return V5ToV4MigrationResult::kPrefixSizeMismatchFailure;
    }
    // Determine the new v4 hash file's path.
    file_size = hash_file.file_size();
    v4_ext = base::NumberToString(v4_prefix_size) + "_" + hash_file.extension();
    v4_hash_file_path = HashPrefixContainer::GetPath(store_path_, v4_ext);

    if (!base::PathExists(v5_hash_file_path)) {
      return V5ToV4MigrationResult::kHashFileMissingFailure;
    }

    // Write to the new hash file.
    if (is_extensions_blocklist_) {
      // For the extensions blocklist, migrate the 16-byte extension hashes to
      // length-16 extension IDs into the v4 hash file, and delete the v5 hash
      // file.
      ConvertExtensionBlocklistV5ToV4Result result =
          ConvertExtensionsBlocklistFromV5ToV4(
              v5_hash_file_path, v4_hash_file_path, v4_checksum, &file_size);
      base::UmaHistogramEnumeration(
          "SafeBrowsing.V4Store.ConvertExtensionBlocklistV5ToV4Result", result);
      if (result != ConvertExtensionBlocklistV5ToV4Result::kSuccess) {
        return V5ToV4MigrationResult::kExtensionBlocklistMigrationFailed;
      }
      base::DeleteFile(v5_hash_file_path);
    } else {
      // For other blocklists, just rename the v5 hash file to v4.
      if (!base::Move(v5_hash_file_path, v4_hash_file_path)) {
        return V5ToV4MigrationResult::kRenameHashFileFailure;
      }
    }
  }

  // Construct the new V4StoreFileFormat proto.
  V4StoreFileFormat v4_file_format;
  v4_file_format.set_magic_number(v5_file_format.magic_number());
  v4_file_format.set_version_number(kV4FileVersion);

  ListUpdateResponse* response = v4_file_format.mutable_list_update_response();
  if (list_details->has_version()) {
    response->set_new_client_state(list_details->version());
  }
  if (v4_checksum) {
    response->mutable_checksum()->set_sha256(std::move(*v4_checksum));
  }
  response->set_response_type(ListUpdateResponse::FULL_UPDATE);

  if (list_details->has_hash_file()) {
    HashFile* hash_file = v4_file_format.add_hash_files();
    hash_file->set_prefix_size(v4_prefix_size);
    hash_file->set_extension(v4_ext);
    hash_file->set_file_size(file_size);
  }

  // Serialize and write the new V4 proto to disk.
  std::string v4_file_format_string;
  if (!v4_file_format.SerializeToString(&v4_file_format_string)) {
    return V5ToV4MigrationResult::kProtoSerializationFailure;
  }

  if (!base::WriteFile(temp_store_path, v4_file_format_string)) {
    return V5ToV4MigrationResult::kWriteV4FileFailure;
  }

  if (!base::Move(temp_store_path, store_path_)) {
    return V5ToV4MigrationResult::kRenameV4StoreFileFailure;
  }

  migration_succeeded = true;

  // Delete the old V5 store file.
  base::DeleteFile(v5_store_path);

  return V5ToV4MigrationResult::kV5ToV4MigrationSucceeded;
}

bool V4Store::WipeV5Store(const base::FilePath& v5_store_path) {
  V5StoreFileFormat v5_file_format;
  bool hash_delete_success = true;
  if (ParseAndValidateV5StoreFileFormat(v5_store_path, v5_file_format) ==
      V5StoreReadResult::kReadSuccess) {
    const auto& list_details = v5_file_format.list_details();
    if (list_details.has_hash_file()) {
      base::FilePath v5_hash_file_path = HashPrefixContainer::GetPath(
          v5_store_path, list_details.hash_file().extension());
      hash_delete_success = base::DeleteFile(v5_hash_file_path);
    }
  }
  bool store_delete_success = base::DeleteFile(v5_store_path);
  return hash_delete_success && store_delete_success;
}

StoreWriteResult V4Store::WriteToDisk(const Checksum& checksum) {
  V4StoreFileFormat file_format;
  ListUpdateResponse* lur = file_format.mutable_list_update_response();
  *(lur->mutable_checksum()) = checksum;
  lur->set_new_client_state(state_);
  lur->set_response_type(ListUpdateResponse::FULL_UPDATE);
  return WriteToDisk(&file_format);
}

StoreWriteResult V4Store::WriteToDisk(V4StoreFileFormat* file_format) {
  base::expected<int64_t, SBStoreWriteResult> file_size_or_error =
      WriteToDiskLoop(
          store_path_, file_format, hash_prefix_map_.get(),
          /*set_file_metadata=*/
          [file_format] {
            file_format->set_magic_number(kFileMagic);
            file_format->set_version_number(kV4FileVersion);
          },
          /*cleanup_on_error=*/
          [this, file_format](const base::FilePath& temp_file) {
            base::DeleteFile(temp_file);
            for (const auto& hash_file : file_format->hash_files()) {
              base::DeleteFile(
                  HashPrefixMap::GetPath(store_path_, hash_file.extension()));
            }
          },
          /*get_hash_files_size=*/
          [file_format] {
            int64_t size = 0;
            for (const auto& hash_file : file_format->hash_files()) {
              size += hash_file.file_size();
            }
            return size;
          },
          /*cleanup_extra_files=*/
          [this, file_format] {
            CleanupExtraFiles(store_path_, *file_format);
          });

  if (file_size_or_error.has_value()) {
    // Update |file_size_| now because we wrote the file correctly.
    file_size_ = file_size_or_error.value();
    return WRITE_SUCCESS;
  }

  // Map SBStoreWriteResult to StoreWriteResult.
  switch (file_size_or_error.error()) {
    case SBStoreWriteResult::kUnexpectedBytesWrittenFailure:
      return UNEXPECTED_BYTES_WRITTEN_FAILURE;
    case SBStoreWriteResult::kUnexpectedWriteFailure:
      return UNEXPECTED_WRITE_FAILURE;
    case SBStoreWriteResult::kUnableToRenameFailure:
      return UNABLE_TO_RENAME_FAILURE;
  }
}

HashPrefixStr V4Store::GetMatchingHashPrefix(const FullHashStr& full_hash) {
  return GetMatchingHashPrefix(std::string_view(full_hash));
}

HashPrefixStr V4Store::GetMatchingHashPrefix(std::string_view full_hash) {
  // It should never be the case that more than one hash prefixes match a given
  // full hash. However, if that happens, this method returns any one of them.
  // It does not guarantee which one of those will be returned.
  DCHECK(full_hash.size() == 32u || full_hash.size() == 21u);
  checks_attempted_++;
  return hash_prefix_map_->GetMatchingHashPrefix(full_hash);
}

bool V4Store::VerifyChecksumFast(const HashPrefixMapView& map_view) {
  crypto::hash::Hasher checksum_ctx(crypto::hash::HashKind::kSha256);
  auto map_pair = *map_view.begin();
  checksum_ctx.Update(base::as_byte_span(map_pair.second));

  std::array<uint8_t, crypto::hash::kSha256Size> checksum;
  checksum_ctx.Finish(checksum);
  if (base::as_byte_span(expected_checksum_) != checksum) {
    RecordApplyUpdateResult(kReadFromDisk, CHECKSUM_MISMATCH_FAILURE,
                            store_path_);
#if DCHECK_IS_ON()
    std::string checksum_b64 = base::Base64Encode(checksum);
    std::string expected_checksum_b64 = base::Base64Encode(expected_checksum_);
    DVLOG(1) << "Failure: Checksum mismatch: calculated: " << checksum_b64
             << "; expected: " << expected_checksum_b64 << "; store: " << *this;
#endif
    return false;
  }
  return true;
}

bool V4Store::VerifyChecksum() {
  base::ElapsedThreadTimer thread_timer;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // TODO(crbug.com/362791941): Remove this once have confirmed assumption that
  // empty checksums are rare for valid stores.
  if (has_valid_data_) {
    base::UmaHistogramBoolean(
        "SafeBrowsing.V4Store.VerifyChecksum.ValidStoreChecksumEmpty" +
            GetUmaSuffixForStore(store_path_),
        expected_checksum_.empty());
    base::UmaHistogramBoolean(
        "SafeBrowsing.V4Store.VerifyChecksum.ValidStoreChecksumEmpty",
        expected_checksum_.empty());
  }

  if (expected_checksum_.empty()) {
    // Nothing to check here folks!
    // TODO(vakh): Do not allow empty checksums.
    return true;
  }

  // Fast path when there is only 1 prefix size.
  const HashPrefixMapView map_view = hash_prefix_map_->view();
  if (map_view.size() == 1) {
    bool result = VerifyChecksumFast(map_view);
    RecordVerifyChecksumDuration(thread_timer.Elapsed());
    return result;
  }

  IteratorMap iterator_map;
  HashPrefixStr next_smallest_prefix;
  InitializeIteratorMap(map_view, &iterator_map);
  CHECK_EQ(map_view.size(), iterator_map.size());
  bool has_unmerged = GetNextSmallestUnmergedPrefix(map_view, iterator_map,
                                                    &next_smallest_prefix);

  crypto::hash::Hasher checksum_ctx(crypto::hash::HashKind::kSha256);
  while (has_unmerged) {
    PrefixSize next_smallest_prefix_size = next_smallest_prefix.size();

    // Update the iterator map, which means that we have read one hash
    // prefix of size |next_smallest_prefix_size| from hash_prefix_map_.
    iterator_map[next_smallest_prefix_size] += next_smallest_prefix_size;

    checksum_ctx.Update(next_smallest_prefix);

    // Find the next smallest unmerged element in the map.
    has_unmerged = GetNextSmallestUnmergedPrefix(map_view, iterator_map,
                                                 &next_smallest_prefix);
  }

  std::array<uint8_t, crypto::hash::kSha256Size> checksum;
  checksum_ctx.Finish(checksum);
  auto expected = base::as_byte_span(expected_checksum_);
  if (expected != checksum) {
    RecordApplyUpdateResult(kReadFromDisk, CHECKSUM_MISMATCH_FAILURE,
                            store_path_);
#if DCHECK_IS_ON()
    std::string checksum_b64 = base::Base64Encode(base::as_byte_span(checksum));
    std::string expected_checksum_b64 = base::Base64Encode(expected_checksum_);
    DVLOG(1) << "Failure: Checksum mismatch: calculated: " << checksum_b64
             << "; expected: " << expected_checksum_b64 << "; store: " << *this;
#endif
    RecordVerifyChecksumDuration(thread_timer.Elapsed());
    return false;
  }

  RecordVerifyChecksumDuration(thread_timer.Elapsed());
  return true;
}

void V4Store::CollectStoreInfo(
    DatabaseManagerInfo::DatabaseInfo::StoreInfo* store_info) {
  SBStore::CollectStoreInfo(store_info);
  store_info->set_v4_update_status(static_cast<int>(last_apply_update_result_));
  store_info->set_checks_attempted(checks_attempted_);
  hash_prefix_map_->GetPrefixInfo(store_info->mutable_prefix_sets());
}

const std::string& V4Store::GetStoreState() const {
  return state_;
}

}  // namespace safe_browsing
