// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_STORAGE_PARTITION_CONFIG_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_STORAGE_PARTITION_CONFIG_H_

#include <string>

#include "components/download/public/common/download_export.h"

namespace download {

// This class represents a content::StoragePartitionConfig that can be within
// components/download. It contains methods for serializing the
// StoragePartitionConfig for storage in the downloads table and to deserialize
// from storage back into this class. It also enables the configs to be compared
// to each other.
class COMPONENTS_DOWNLOAD_EXPORT StoragePartitionConfig {
 public:
  StoragePartitionConfig() = default;
  StoragePartitionConfig(const std::string& partition_domain,
                         const std::string& partition_name,
                         bool in_memory);

  StoragePartitionConfig(const StoragePartitionConfig& rhs) = default;
  StoragePartitionConfig& operator=(const StoragePartitionConfig& rhs) =
      default;

  bool operator==(const StoragePartitionConfig& rhs) const;
  bool operator!=(const StoragePartitionConfig& rhs) const;

  // Serialize and Deserialize methods to store the StoragePartitionConfig in
  // the DownloadDatabase.
  std::string SerializeToString();
  static StoragePartitionConfig DeserializeFromString(const std::string& str);

  std::string partition_domain() const { return partition_domain_; }
  std::string partition_name() const { return partition_name_; }
  bool in_memory() const { return in_memory_; }

 private:
  std::string partition_domain_;
  std::string partition_name_;
  bool in_memory_;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_STORAGE_PARTITION_CONFIG_H_
