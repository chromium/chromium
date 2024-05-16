// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/telemetry_extension/diagnostics/mojo_utils.h"

#include <string>

#include "base/files/file.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace ash::converters::diagnostics {

// static
std::string MojoUtils::GetStringFromMojoHandle(mojo::ScopedHandle handle) {
  base::ScopedPlatformFile platform_file;
  auto result = mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  if (result != MOJO_RESULT_OK) {
    return "";
  }

  base::File file(std::move(platform_file));
  size_t file_size = 0;
  {
    // TODO(b/322741627): Remove blocking operation from production code.
    base::ScopedAllowBlocking allow_blocking;
    file_size = file.GetLength();
  }
  if (file_size <= 0) {
    return "";
  }

  base::subtle::PlatformSharedMemoryRegion platform_region =
      base::subtle::PlatformSharedMemoryRegion::Take(
          base::ScopedFD(file.TakePlatformFile()),
          base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly, file_size,
          base::UnguessableToken::Create());

  base::ReadOnlySharedMemoryRegion shm_region =
      base::ReadOnlySharedMemoryRegion::Deserialize(std::move(platform_region));
  base::ReadOnlySharedMemoryMapping shm_mapping = shm_region.Map();
  if (!shm_mapping.IsValid()) {
    return "";
  }
  return std::string(shm_mapping.GetMemoryAs<char>(), shm_mapping.size());
}

}  // namespace ash::converters::diagnostics
