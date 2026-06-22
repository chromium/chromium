// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/platform_runtime/platform_runtime_impl.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/scoped_native_library.h"
#include "base/synchronization/lock.h"

namespace platform_runtime {

PlatformRuntimeLibrary::PlatformRuntimeLibrary(
    base::ScopedNativeLibrary library)
    : library_(std::move(library)) {
  // TODO(crbug.com/513193869): Retrieve and store function pointers here.
}

PlatformRuntimeLibrary::~PlatformRuntimeLibrary() = default;

// static
PlatformRuntimeImpl* PlatformRuntimeImpl::GetInstance() {
  static base::NoDestructor<PlatformRuntimeImpl> instance;
  return instance.get();
}

PlatformRuntimeImpl::PlatformRuntimeImpl() = default;
PlatformRuntimeImpl::~PlatformRuntimeImpl() = default;

void PlatformRuntimeImpl::UpdatePlatformRuntimeLibrary(
    const base::FilePath& library_path) {
  if (library_path.empty()) {
    base::AutoLock lock(lock_);
    loaded_library_ = nullptr;
    return;
  }

  base::NativeLibraryLoadError error;
  base::NativeLibrary native_lib =
      base::LoadNativeLibrary(library_path, &error);
  if (!native_lib) {
    DLOG(ERROR) << "Failed to load Platform Runtime library: "
                << error.ToString();
    return;
  }

  auto new_library = base::MakeRefCounted<PlatformRuntimeLibrary>(
      base::ScopedNativeLibrary(native_lib));
  scoped_refptr<PlatformRuntimeLibrary> old_library;
  {
    base::AutoLock lock(lock_);
    old_library = std::move(loaded_library_);
    loaded_library_ = std::move(new_library);
  }
}

scoped_refptr<PlatformRuntimeLibrary> PlatformRuntimeImpl::GetLoadedLibrary() {
  base::AutoLock lock(lock_);
  return loaded_library_;
}

}  // namespace platform_runtime
