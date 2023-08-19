// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_file_data_source.h"

#include <limits>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_math.h"
#include "mojo/public/cpp/system/file_data_source.h"

namespace web_package {

std::unique_ptr<WebBundleFileDataSource>
WebBundleFileDataSource::CreateDataSource(base::File file,
                                          uint64_t offset,
                                          uint64_t length) {
  return base::WrapUnique(
      new WebBundleFileDataSource(std::move(file), offset, length));
}

WebBundleFileDataSource::WebBundleFileDataSource(base::File file,
                                                 uint64_t offset,
                                                 uint64_t length)
    : file_(std::move(file)), offset_(offset), length_(length) {
  error_ =
      mojo::FileDataSource::ConvertFileErrorToMojoResult(file_.error_details());

  // base::File::Read takes int64_t as an offset. So, offset + length should
  // not overflow in int64_t.
  uint64_t max_offset;
  if (!base::CheckAdd(offset, length).AssignIfValid(&max_offset) ||
      (std::numeric_limits<int64_t>::max() < max_offset)) {
    error_ = MOJO_RESULT_INVALID_ARGUMENT;
  }
}

WebBundleFileDataSource::~WebBundleFileDataSource() = default;

uint64_t WebBundleFileDataSource::GetLength() const {
  return length_;
}

WebBundleFileDataSource::ReadResult WebBundleFileDataSource::Read(
    uint64_t offset,
    base::span<char> buffer) {
  ReadResult result;
  result.result = error_;

  if (length_ < offset) {
    result.result = MOJO_RESULT_INVALID_ARGUMENT;
  }

  if (result.result != MOJO_RESULT_OK) {
    return result;
  }

  uint64_t readable_size = length_ - offset;
  uint64_t writable_size = buffer.size();
  uint64_t copyable_size =
      std::min(std::min(readable_size, writable_size),
               static_cast<uint64_t>(std::numeric_limits<int>::max()));

  int bytes_read = file_.Read(offset_ + offset, buffer.data(), copyable_size);
  if (bytes_read < 0) {
    result.result = mojo::FileDataSource::ConvertFileErrorToMojoResult(
        file_.GetLastFileError());
  } else {
    result.bytes_read = bytes_read;
  }
  return result;
}

}  // namespace web_package
