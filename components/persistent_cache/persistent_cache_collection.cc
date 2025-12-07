// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/persistent_cache/persistent_cache_collection.h"

#include <stddef.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/map_util.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/persistent_cache/backend_storage.h"
#include "components/persistent_cache/backend_type.h"
#include "components/persistent_cache/lock_state.h"
#include "components/persistent_cache/pending_backend.h"
#include "components/persistent_cache/persistent_cache.h"
#include "components/persistent_cache/transaction_error.h"

namespace persistent_cache {

PersistentCacheCollection::PersistentCacheCollection(
    base::FilePath top_directory,
    int64_t target_footprint,
    size_t lru_capacity)
    : backend_storage_(BackendType::kSqlite, std::move(top_directory)),
      target_footprint_(target_footprint),
      lru_capacity_(lru_capacity),
      persistent_caches_(PersistentCacheLRUMap::NO_AUTO_EVICT) {
  ReduceFootPrint();
}

PersistentCacheCollection::PersistentCacheCollection(
    base::FilePath top_directory,
    int64_t target_footprint,
    std::unique_ptr<BackendStorage::Delegate> storage_delegate,
    size_t lru_capacity)
    : backend_storage_(std::move(storage_delegate), std::move(top_directory)),
      target_footprint_(target_footprint),
      lru_capacity_(lru_capacity),
      persistent_caches_(PersistentCacheLRUMap::NO_AUTO_EVICT) {
  ReduceFootPrint();
}

PersistentCacheCollection::~PersistentCacheCollection() = default;

base::expected<std::optional<EntryMetadata>, TransactionError>
PersistentCacheCollection::Find(const std::string& cache_id,
                                std::string_view key,
                                BufferProvider buffer_provider) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* cache = GetOrCreateCache(cache_id);
  if (!cache) {
    return base::unexpected(TransactionError::kPermanent);
  }

  ASSIGN_OR_RETURN(auto entry, cache->Find(key, buffer_provider),
                   [&cache_id, this](TransactionError error) {
                     return HandleTransactionError(cache_id, error);
                   });
  return entry;
}

base::expected<void, TransactionError> PersistentCacheCollection::Insert(
    const std::string& cache_id,
    std::string_view key,
    base::span<const uint8_t> content,
    EntryMetadata metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Approximate the footprint of this insert to the size of the key and value
  // combined. This is optimistic in some ways since it doesn't account for any
  // overhead and pessimimistic as it assumes every single write is both new and
  // doesn't evict something else.
  bytes_until_footprint_reduction_ -= key.size() + content.size();
  if (bytes_until_footprint_reduction_ <= 0) {
    ReduceFootPrint();
  }

  auto* cache = GetOrCreateCache(cache_id);
  if (!cache) {
    return base::unexpected(TransactionError::kPermanent);
  }

  RETURN_IF_ERROR(cache->Insert(key, content, metadata),
                  [&cache_id, this](TransactionError error) {
                    return HandleTransactionError(cache_id, error);
                  });

  return base::ok();
}

void PersistentCacheCollection::DeleteAllFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Delete all files. Backends open all files with FLAG_WIN_SHARE_DELETE so
  // that they can be deleted even while open. Doing this before closing them
  // avoids a race condition where a scanner may try to open written-to files
  // immediately after they have been closed.
  backend_storage_.DeleteAllFiles();

  // Clear all managed persistent caches so that they close their files, thereby
  // allowing them to be deleted.
  Clear();
}

void PersistentCacheCollection::AbandonCache(
    const std::string& cache_id,
    PersistentCache* persistent_cache) {
  if (persistent_cache->Abandon() != LockState::kNotHeld) {
    // Some connections had a view of the files while abandoning. These could
    // be zombie connections but more importantly hung clients that will
    // eventually attempt to use the data again on resuming. Unlink files so
    // clients retain a stable view of them and new files with the same name
    // can be recreated if needed.
    backend_storage_.DeleteFiles(BaseNameFromCacheId(cache_id));
  }
}

std::optional<PendingBackend>
PersistentCacheCollection::ShareReadOnlyConnection(
    const std::string& cache_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto* cache = GetOrCreateCache(cache_id); cache) {
    return backend_storage_.ShareReadOnlyConnection(
        BaseNameFromCacheId(cache_id), *cache);
  }
  return std::nullopt;
}

std::optional<PendingBackend>
PersistentCacheCollection::ShareReadWriteConnection(
    const std::string& cache_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto* cache = GetOrCreateCache(cache_id); cache) {
    return backend_storage_.ShareReadWriteConnection(
        BaseNameFromCacheId(cache_id), *cache);
  }
  return std::nullopt;
}

TransactionError PersistentCacheCollection::HandleTransactionError(
    const std::string& cache_id,
    TransactionError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = persistent_caches_.Get(cache_id);
  CHECK(it != persistent_caches_.end());

  switch (error) {
    case TransactionError::kTransient:
      // Nothing to do in the case of transient errors but report them.
      break;
    case TransactionError::kConnectionError:
      // Remove from opened caches to force re-opening since the files are
      // presumed still valid.
      AbandonCache(it->first, it->second.get());
      persistent_caches_.Erase(it);
      break;
    case TransactionError::kPermanent:
      // Delete files as they cannot be used again. Purposefully done before
      // erasing the instance to avoid races. See `DeleteAllFiles()` for
      // details.
      backend_storage_.DeleteFiles(BaseNameFromCacheId(cache_id));

      // Remove from opened caches since the backing files don't exist anymore.
      AbandonCache(it->first, it->second.get());
      persistent_caches_.Erase(it);
      break;
  }

  return error;
}

void PersistentCacheCollection::ReduceFootPrint() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear all managed persistent caches so they don't hold on to files or
  // prevent their deletion.
  Clear();

  // Reducing the footprint of the collection to exactly the desired target
  // could have the effect of rapidly going over the limit again. This might end
  // up issuing more reductions than desirable. This defines some headroom to
  // try and mitigate the issue.
  constexpr double kFootPrintReductionFactor = 0.90;

  auto adjusted_target = target_footprint_ * kFootPrintReductionFactor;
  auto current_footprint =
      backend_storage_.BringDownTotalFootprintOfFiles(adjusted_target)
          .current_footprint;

  bytes_until_footprint_reduction_ = target_footprint_ - current_footprint;
}

PersistentCache* PersistentCacheCollection::GetOrCreateCache(
    const std::string& cache_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = persistent_caches_.Get(cache_id);

  // If the cache is already created.
  if (it != persistent_caches_.end()) {
    return it->second.get();
  }

  // The cache would exceed capacity on next insert. Remove the oldest entry to
  // make room.
  if (persistent_caches_.size() == lru_capacity_) {
    // Get the oldest entry iterator;
    auto oldest_it = persistent_caches_.rbegin();
    AbandonCache(oldest_it->first, oldest_it->second.get());
    persistent_caches_.Erase(oldest_it);
  }

  base::FilePath base_name = BaseNameFromCacheId(cache_id);
  // `cache_id` must not contain invalid characters.
  CHECK(!base_name.empty());

  auto backend =
      backend_storage_.MakeBackend(base_name, /*single_connection=*/false,
                                   /*journal_mode_wal=*/false);
  if (!backend) {
    // Failed to open/create the backend's files or bind to them.
    return nullptr;
  }

  // Create the cache
  auto inserted_it = persistent_caches_.Put(
      cache_id, std::make_unique<PersistentCache>(std::move(backend)));
  return inserted_it->second.get();
}

void PersistentCacheCollection::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& [cache_id, cache] : persistent_caches_) {
    AbandonCache(cache_id, cache.get());
  }
  persistent_caches_.Clear();
}

namespace {

// All characters allowed in filenames.
constexpr auto kAllowedCharsInFilenames = base::MakeFixedFlatSet<char>(
    base::sorted_unique,
    {' ', '!', '#', '$', '&', '\'', '(', ')', '+', ',', '-', '.', '0', '1',
     '2', '3', '4', '5', '6', '7',  '8', '9', ';', '=', '@', '[', ']', '_',
     'a', 'b', 'c', 'd', 'e', 'f',  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
     'o', 'p', 'q', 'r', 's', 't',  'u', 'v', 'w', 'x', 'y', 'z', '~'});

static_assert(kAllowedCharsInFilenames.size() < 128,
              "Allowed chars are a subset of ASCII and overflow while "
              "indexing should never be a worry");

// Returns an arbitrary character at a fixed offset from `c` in the dictionary
// above, or an empty value if not present in the dictionary.
std::optional<char> RotateChar(char c) {
  auto char_iter = kAllowedCharsInFilenames.find(c);

  // Characters illegal in filenames are not handled in this function.
  if (char_iter == kAllowedCharsInFilenames.end()) {
    return std::nullopt;
  }

  auto char_index = char_iter - kAllowedCharsInFilenames.begin();

  // Arbitrary offset to rotate index in the list of allowed characters.
  static constexpr int kRotationOffset = 37;

  // Use a rotating index to find a character to replace `c`.
  auto target_index =
      (char_index + kRotationOffset) % kAllowedCharsInFilenames.size();
  return *(kAllowedCharsInFilenames.begin() + target_index);
}

// Mapping of characters illegal in filenames to a unique token to represent
// them in filenames. This prevents collisions by avoiding mapping two
// characters to the same value. Ex:
// "*/" --> "`9`2"
// "><" --> "`5`4"
//
// Mapping both strings to "`1`1" for example would result in a valid filename
// but in backing files being shared for two keys which is not correct.
constexpr auto kCharacterToTokenMap =
    base::MakeFixedFlatMap<char, std::string_view>({{'\\', "`1"},
                                                    {'/', "`2"},
                                                    {'|', "`3"},
                                                    {'<', "`4"},
                                                    {'>', "`5"},
                                                    {':', "`6"},
                                                    {'\"', "`7"},
                                                    {'?', "`8"},
                                                    {'*', "`9"},
                                                    {'\n', "`0"}});

// Returns a token uniquely representing a character `c` that is not legal in
// filenames, or an empty string if no such replacement is available.
std::string_view FilenameIllegalCharToReplacementToken(char c) {
  if (const auto* value = base::FindOrNull(kCharacterToTokenMap, c); value) {
    return *value;
  }
  return {};
}

}  // namespace

// static
base::FilePath PersistentCacheCollection::BaseNameFromCacheId(
    const std::string& cache_id) {
  std::string filename;

  // Optimistically reserve enough space assuming there are no illegal
  // characters in `cache_id`.
  filename.reserve(cache_id.size());
  for (char c : cache_id) {
    if (auto rotated_char = RotateChar(c); rotated_char.has_value()) {
      filename.push_back(*rotated_char);
    } else if (auto token = FilenameIllegalCharToReplacementToken(c);
               !token.empty()) {
      filename += token;
    } else {
      // There's no way to rotate an illegal character so return an empty
      // path.
      return base::FilePath();
    }
  }

  return base::FilePath::FromASCII(std::move(filename));
}

// static
std::string PersistentCacheCollection::GetAllAllowedCharactersInCacheIds() {
  std::string result;
  for (auto c : kAllowedCharsInFilenames) {
    result.push_back(c);
  }
  for (const auto& [c, replacement] : kCharacterToTokenMap) {
    result.push_back(c);
  }
  return result;
}

}  // namespace persistent_cache
