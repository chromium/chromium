// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage_configuration.h"

namespace reporting {

StorageOptions::StorageOptions() = default;
StorageOptions::StorageOptions(const StorageOptions& options) = default;
StorageOptions& StorageOptions::operator=(const StorageOptions& options) =
    default;
StorageOptions::~StorageOptions() = default;

QueueOptions::QueueOptions(const StorageOptions& storage_options)
    : storage_options_(storage_options) {}
QueueOptions::QueueOptions(const QueueOptions& options) = default;
}  // namespace reporting
