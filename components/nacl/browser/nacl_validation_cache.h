// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROWSER_NACL_VALIDATION_CACHE_H_
#define COMPONENTS_NACL_BROWSER_NACL_VALIDATION_CACHE_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "base/containers/lru_cache.h"

namespace base {
class Pickle;
}

namespace nacl {

class NaClValidationCache {
 public:
  NaClValidationCache();

  NaClValidationCache(const NaClValidationCache&) = delete;
  NaClValidationCache& operator=(const NaClValidationCache&) = delete;

  ~NaClValidationCache();

  // Get the key used for HMACing validation signatures.  This should be a
  // string of cryptographically secure random bytes.
  const std::string& GetValidationCacheKey() const {
    return validation_cache_key_;
  }

  // Is the validation signature in the database?
  bool QueryKnownToValidate(const std::string& signature, bool reorder);

  // Put the validation signature in the database.
  void SetKnownToValidate(const std::string& signature);

  void Reset();
  void Serialize(base::Pickle* pickle) const;
  bool Deserialize(const base::Pickle* pickle);

  // Testing functions
  size_t size() const {
    return validation_cache_.size();
  }
  void SetValidationCacheKey(std::string& key) {
    validation_cache_key_ = key;
  }
  std::vector<std::string> GetContents() const {
    std::vector<std::string> contents;
    ValidationCacheType::const_iterator iter = validation_cache_.begin();
    for (iter = validation_cache_.begin();
         iter != validation_cache_.end();
         iter++) {
      contents.push_back(iter->first);
    }
    return contents;
  }

 private:
  bool DeserializeImpl(const base::Pickle* pickle);

  typedef base::HashingLRUCache<std::string, bool> ValidationCacheType;
  ValidationCacheType validation_cache_;

  std::string validation_cache_key_;
};

} // namespace nacl

#endif  // COMPONENTS_NACL_BROWSER_NACL_VALIDATION_CACHE_H_
