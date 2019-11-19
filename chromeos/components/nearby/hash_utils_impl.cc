// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/hash_utils_impl.h"

#include "base/hash/md5.h"
#include "base/memory/ptr_util.h"
#include "crypto/sha2.h"

namespace chromeos {

namespace nearby {

HashUtilsImpl::HashUtilsImpl() = default;
HashUtilsImpl::~HashUtilsImpl() = default;

std::unique_ptr<location::nearby::ByteArray> HashUtilsImpl::md5(
    const std::string& input) {
  std::string md5_result = base::MD5String(input);
  return std::make_unique<location::nearby::ByteArray>(md5_result.data(),
                                                       md5_result.size());
}

std::unique_ptr<location::nearby::ByteArray> HashUtilsImpl::sha256(
    const std::string& input) {
  std::string sha256_result = crypto::SHA256HashString(input);
  return std::make_unique<location::nearby::ByteArray>(sha256_result.data(),
                                                       sha256_result.size());
}

}  // namespace nearby

}  // namespace chromeos
