// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/hashing_utils.h"

#include <array>
#include <string>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/page_size.h"
#include "base/threading/scoped_blocking_call.h"
#include "crypto/hash.h"

namespace device_signals {

std::optional<std::string> HashFile(const base::FilePath& file_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return std::nullopt;
  }

  crypto::hash::Hasher hash(crypto::hash::HashKind::kSha256);
  auto buffer = base::HeapArray<uint8_t>::Uninit(base::GetPageSize());

  std::optional<size_t> bytes_read;
  do {
    bytes_read = file.ReadAtCurrentPos(buffer.as_span());
    if (!bytes_read.has_value()) {
      return std::nullopt;
    }
    hash.Update(buffer.first(*bytes_read));
  } while (bytes_read.value() > 0);

  std::string result(crypto::hash::kSha256Size, 0);
  hash.Finish(base::as_writable_byte_span(result));
  return result;
}

}  // namespace device_signals
