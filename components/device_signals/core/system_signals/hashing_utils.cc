// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/hashing_utils.h"

#include <array>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/page_size.h"
#include "base/threading/scoped_blocking_call.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

namespace device_signals {

std::optional<std::string> HashFile(const base::FilePath& file_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return std::nullopt;
  }

  auto secure_hash =
      crypto::SecureHash::Create(crypto::SecureHash::Algorithm::SHA256);
  std::vector<char> buffer(base::GetPageSize());

  std::optional<size_t> bytes_read;
  do {
    bytes_read = file.ReadAtCurrentPos(base::as_writable_byte_span(buffer));
    if (!bytes_read.has_value()) {
      return std::nullopt;
    }
    secure_hash->Update(buffer.data(), bytes_read.value());
  } while (bytes_read.value() > 0);

  std::string hash(crypto::kSHA256Length, 0);
  secure_hash->Finish(std::data(hash), hash.size());
  return hash;
}

}  // namespace device_signals
