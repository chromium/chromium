// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crypto/signature_cache.h"


namespace chromecast {

namespace {
const int kSignatureCacheSize = 10;
}  // namespace

SignatureCache::SignatureCache()
    : contents_(kSignatureCacheSize) {}

SignatureCache::~SignatureCache() {}

std::string SignatureCache::Get(const std::string& wrapped_private_key,
                                const std::string& hash) {
  std::string result;
  base::AutoLock lock(lock_);

  if (wrapped_private_key != key_)
    return result;

  const auto iter = contents_.Get(hash);
  if (iter != contents_.end())
    result = iter->second;

  return result;
}

void SignatureCache::Put(const std::string& wrapped_private_key,
                         const std::string& hash,
                         const std::string& signature) {
  base::AutoLock lock(lock_);

  if (wrapped_private_key != key_) {
    key_ = wrapped_private_key;
    contents_.Clear();
  }

  contents_.Put(hash, signature);
}

}  // namespace chromecast
