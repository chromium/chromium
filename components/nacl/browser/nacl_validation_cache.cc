// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/browser/nacl_validation_cache.h"

#include "base/containers/adapters.h"
#include "base/pickle.h"
#include "base/rand_util.h"

namespace nacl {

// For the moment, choose an arbitrary cache size.
const size_t kValidationCacheCacheSize = 500;
// Key size is equal to the block size (not the digest size) of SHA256.
const size_t kValidationCacheKeySize = 64;
// Entry size is equal to the digest size of SHA256.
const size_t kValidationCacheEntrySize = 32;

const char kValidationCacheBeginMagic[] = "NaCl";
const char kValidationCacheEndMagic[] = "Done";

NaClValidationCache::NaClValidationCache()
    : validation_cache_(kValidationCacheCacheSize) {
  // Make sure the cache key is unpredictable, even if the cache has not
  // been loaded.
  Reset();
}

NaClValidationCache::~NaClValidationCache() {
  // Make clang's style checking happy by adding a destructor.
}

bool NaClValidationCache::QueryKnownToValidate(const std::string& signature,
                                               bool reorder) {
  if (signature.length() == kValidationCacheEntrySize) {
    ValidationCacheType::iterator iter;
    if (reorder) {
      iter = validation_cache_.Get(signature);
    } else {
      iter = validation_cache_.Peek(signature);
    }
    if (iter != validation_cache_.end()) {
      return iter->second;
    }
  }
  return false;
}

void NaClValidationCache::SetKnownToValidate(const std::string& signature) {
  if (signature.length() == kValidationCacheEntrySize) {
    validation_cache_.Put(signature, true);
  }
}

void NaClValidationCache::Serialize(base::Pickle* pickle) const {
  // Mark the beginning of the data stream.
  pickle->WriteString(kValidationCacheBeginMagic);
  pickle->WriteString(validation_cache_key_);
  pickle->WriteInt(validation_cache_.size());

  // Serialize the cache in reverse order so that deserializing it can easily
  // preserve the MRU order.  (Last item deserialized => most recently used.)
  ValidationCacheType::const_reverse_iterator iter;
  for (const auto& [signature, value] : base::Reversed(validation_cache_)) {
    pickle->WriteString(signature);
  }

  // Mark the end of the data stream.
  pickle->WriteString(kValidationCacheEndMagic);
}

void NaClValidationCache::Reset() {
  validation_cache_key_ = base::RandBytesAsString(kValidationCacheKeySize);
  validation_cache_.Clear();
}

bool NaClValidationCache::Deserialize(const base::Pickle* pickle) {
  bool success = DeserializeImpl(pickle);
  if (!success) {
    Reset();
  }
  return success;
}

bool NaClValidationCache::DeserializeImpl(const base::Pickle* pickle) {
  base::PickleIterator iter(*pickle);
  std::string buffer;
  int count;

  // Magic
  if (!iter.ReadString(&buffer))
    return false;
  if (0 != buffer.compare(kValidationCacheBeginMagic))
    return false;

  // Key
  if (!iter.ReadString(&buffer))
    return false;
  if (buffer.size() != kValidationCacheKeySize)
    return false;

  validation_cache_key_ = buffer;
  validation_cache_.Clear();

  // Cache entries
  if (!iter.ReadInt(&count))
    return false;
  for (int i = 0; i < count; ++i) {
    if (!iter.ReadString(&buffer))
      return false;
    if (buffer.size() != kValidationCacheEntrySize)
      return false;
    validation_cache_.Put(buffer, true);
  }

  // Magic
  if (!iter.ReadString(&buffer))
    return false;
  if (0 != buffer.compare(kValidationCacheEndMagic))
    return false;

  // Success!
  return true;
}

}  // namespace nacl
