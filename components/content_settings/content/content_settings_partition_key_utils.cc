// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/content/content_settings_partition_key_utils.h"

#include "base/feature_list.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/features.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"

namespace content_settings {

PartitionKey GetPartitionKey(const content::StoragePartitionConfig& config) {
  return PartitionKey();
}

PartitionKey GetPartitionKey(const content::StoragePartition& partition) {
  return GetPartitionKey(partition.GetConfig());
}

}  // namespace content_settings
