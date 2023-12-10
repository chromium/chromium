// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CONTENT_CONTENT_SETTINGS_PARTITION_KEY_UTILS_H_
#define COMPONENTS_CONTENT_SETTINGS_CONTENT_CONTENT_SETTINGS_PARTITION_KEY_UTILS_H_

#include "components/content_settings/core/common/content_settings_partition_key.h"

namespace content {
class StoragePartitionConfig;
class StoragePartition;
}  // namespace content

namespace content_settings {

PartitionKey GetPartitionKey(const content::StoragePartitionConfig& config);

PartitionKey GetPartitionKey(const content::StoragePartition& partition);

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CONTENT_CONTENT_SETTINGS_PARTITION_KEY_UTILS_H_
