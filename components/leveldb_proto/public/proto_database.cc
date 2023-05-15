// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/public/proto_database.h"

#include "base/system/sys_info.h"

namespace leveldb_proto {
namespace {
const size_t kDatabaseWriteBufferSizeBytes = 512 * 1024;
const size_t kDatabaseWriteBufferSizeBytesForLowEndDevice = 128 * 1024;
}  // namespace

leveldb_env::Options CreateSimpleOptions() {
  leveldb_env::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // Use minimum.
  static bool is_low_end_device =
      base::SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled();
  if (is_low_end_device)
    options.write_buffer_size = kDatabaseWriteBufferSizeBytesForLowEndDevice;
  else
    options.write_buffer_size = kDatabaseWriteBufferSizeBytes;
  return options;
}

}  // namespace leveldb_proto
