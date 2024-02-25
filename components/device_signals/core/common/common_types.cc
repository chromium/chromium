// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/common_types.h"

#include "components/device_signals/core/common/signals_constants.h"

namespace device_signals {

ExecutableMetadata::ExecutableMetadata() = default;

ExecutableMetadata::ExecutableMetadata(const ExecutableMetadata&) = default;
ExecutableMetadata& ExecutableMetadata::operator=(const ExecutableMetadata&) =
    default;

ExecutableMetadata::~ExecutableMetadata() = default;

bool ExecutableMetadata::operator==(const ExecutableMetadata& other) const {
  return is_running == other.is_running &&
         public_keys_hashes == other.public_keys_hashes &&
         product_name == other.product_name && version == other.version;
}

FileSystemItem::FileSystemItem() = default;

FileSystemItem::FileSystemItem(const FileSystemItem&) = default;
FileSystemItem& FileSystemItem::operator=(const FileSystemItem&) = default;

FileSystemItem::~FileSystemItem() = default;

bool FileSystemItem::operator==(const FileSystemItem& other) const {
  return file_path == other.file_path && presence == other.presence &&
         sha256_hash == other.sha256_hash &&
         executable_metadata == other.executable_metadata;
}

GetFileSystemInfoOptions::GetFileSystemInfoOptions() = default;

GetFileSystemInfoOptions::GetFileSystemInfoOptions(
    const GetFileSystemInfoOptions&) = default;
GetFileSystemInfoOptions& GetFileSystemInfoOptions::operator=(
    const GetFileSystemInfoOptions&) = default;

GetFileSystemInfoOptions::~GetFileSystemInfoOptions() = default;

bool GetFileSystemInfoOptions::operator==(
    const GetFileSystemInfoOptions& other) const {
  return file_path == other.file_path &&
         compute_sha256 == other.compute_sha256 &&
         compute_executable_metadata == other.compute_executable_metadata;
}

std::optional<base::Value> CrowdStrikeSignals::ToValue() const {
  if (customer_id.empty() && agent_id.empty()) {
    return std::nullopt;
  }

  base::Value::Dict dict_value;

  if (!customer_id.empty()) {
    dict_value.Set(names::kCustomerId, customer_id);
  }

  if (!agent_id.empty()) {
    dict_value.Set(names::kAgentId, agent_id);
  }

  return base::Value(std::move(dict_value));
}

bool CrowdStrikeSignals::operator==(const CrowdStrikeSignals& other) const {
  return agent_id == other.agent_id && customer_id == other.customer_id;
}

}  // namespace device_signals
