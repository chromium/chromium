// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_PARTITION_KEY_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_PARTITION_KEY_H_

#include <string>

#include "base/no_destructor.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_IOS)
namespace content {
class StoragePartitionConfig;
}  // namespace content
#endif  // BUILDFLAG(IS_IOS)

namespace content_settings {

// The partition key for content settings. Each of the partition is identified
// by the combination of `domain`, `name` and `in_memory`. `in_memory` means
// that the content settings should not be persisted to disk.
//
// The partitioning only applies to user-modifiable content settings (e.g.
// content settings managed by `content_settings::PrefProvider`) but not the
// others (e.g. content settings controlled by policy or extension ).
//
// For non-ios platforms, the partition key is computed directly or indirectly
// from `content::StoragePartitionConfig`. For ios, since partitioning is not
// supported, you can only get the default partition key.
class PartitionKey {
 public:
  friend base::NoDestructor<PartitionKey>;

#if BUILDFLAG(IS_IOS)
  static const PartitionKey& GetDefault();
#else
  friend PartitionKey GetPartitionKey(
      const content::StoragePartitionConfig& config);

  // Get the default PartitionKey for tests. If your test uses non-default
  // StoragePartitions, it should not call this. Instead, call
  // `content_settings::GetPartitionKey()` with appropriate arguments.
  static const PartitionKey& GetDefaultForTesting();
#endif  // BUILDFLAG(IS_IOS)

  // Content settings partitioning is a work-in-progress. When it is done, for
  // non-ios platforms, the partition key is supposed to be computed from
  // StoragePartitionConfig. But for now we need to have this function to help
  // with the migration.
  //
  // TODO(b/307193732): Fix all callers and remove this function.
  static const PartitionKey& WipGetDefault();


  PartitionKey(const PartitionKey& key);
  PartitionKey(PartitionKey&& key);

  // When partitioning is enabled, `domain` and `name` are set to the same
  // values as the StoragePartitionConfig.
  const std::string& domain() const { return domain_; }
  const std::string& name() const { return name_; }

  bool in_memory() const { return in_memory_; }

  bool is_default() const { return domain_.empty(); }

 private:
  PartitionKey();
  PartitionKey(const std::string& domain,
               const std::string& name,
               bool in_memory);
  static const PartitionKey& GetDefaultImpl();

  const std::string domain_;
  const std::string name_;
  const bool in_memory_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_PARTITION_KEY_H_
