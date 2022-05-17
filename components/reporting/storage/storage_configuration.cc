// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage_configuration.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"

namespace reporting {

StorageOptions::StorageOptions()
    : memory_resource_(base::MakeRefCounted<MemoryResourceImpl>(
          4u * 1024uLL * 1024uLL)),  // 4 MiB by default
      disk_space_resource_(base::MakeRefCounted<DiskResourceImpl>(
          64u * 1024uLL * 1024uLL))  // 64 MiB by default.
{}
StorageOptions::StorageOptions(const StorageOptions& options) = default;
StorageOptions::~StorageOptions() = default;

QueueOptions::QueueOptions(const StorageOptions& storage_options)
    : storage_options_(storage_options) {}
QueueOptions::QueueOptions(const QueueOptions& options) = default;
}  // namespace reporting
