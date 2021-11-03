// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/storage_partition_config.h"

#include "base/strings/string_split.h"

namespace download {

namespace {
static const char* kDelimiter = "|";
static const char* kInMemorySetValue = "in_memory";
}  // namespace

StoragePartitionConfig::StoragePartitionConfig(
    const std::string& partition_domain,
    const std::string& partition_name,
    bool in_memory)
    : partition_domain_(partition_domain),
      partition_name_(partition_name),
      in_memory_(in_memory) {
  DCHECK(partition_domain_.find(kDelimiter) == std::string::npos);
  DCHECK(partition_name.find(kDelimiter) == std::string::npos);
}

bool StoragePartitionConfig::operator==(
    const StoragePartitionConfig& rhs) const {
  return partition_domain_ == rhs.partition_domain_ &&
         partition_name_ == rhs.partition_name_ && in_memory_ == rhs.in_memory_;
}

bool StoragePartitionConfig::operator!=(
    const StoragePartitionConfig& rhs) const {
  return !(*this == rhs);
}

std::string StoragePartitionConfig::SerializeToString() {
  return partition_domain_ + kDelimiter + partition_name_ + kDelimiter +
         (in_memory_ ? kInMemorySetValue : "");
}

// static
StoragePartitionConfig StoragePartitionConfig::DeserializeFromString(
    const std::string& str) {
  std::vector<std::string> fields = base::SplitString(
      str, kDelimiter, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  if (fields.size() != 3)
    return StoragePartitionConfig();

  std::string partition_domain = fields[0];
  std::string partition_name = fields[1];
  bool in_memory = fields[2] == kInMemorySetValue;

  return StoragePartitionConfig(partition_domain, partition_name, in_memory);
}

}  // namespace download
