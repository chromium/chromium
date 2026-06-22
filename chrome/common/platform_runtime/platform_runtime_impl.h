// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PLATFORM_RUNTIME_PLATFORM_RUNTIME_IMPL_H_
#define CHROME_COMMON_PLATFORM_RUNTIME_PLATFORM_RUNTIME_IMPL_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/scoped_native_library.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace base {
class FilePath;
}  // namespace base

namespace platform_runtime {

// Encapsulates the loaded library and its function pointers.
// Reference counting ensures it stays alive while in-flight requests are using
// it.
class COMPONENT_EXPORT(PLATFORM_RUNTIME) PlatformRuntimeLibrary
    : public base::RefCountedThreadSafe<PlatformRuntimeLibrary> {
 public:
  explicit PlatformRuntimeLibrary(base::ScopedNativeLibrary library);

  // TODO(crbug.com/513193869): Add function pointers here.

 private:
  friend class base::RefCountedThreadSafe<PlatformRuntimeLibrary>;
  ~PlatformRuntimeLibrary();

  base::ScopedNativeLibrary library_;
};

class COMPONENT_EXPORT(PLATFORM_RUNTIME) PlatformRuntimeImpl {
 public:
  static PlatformRuntimeImpl* GetInstance();

  PlatformRuntimeImpl(const PlatformRuntimeImpl&) = delete;
  PlatformRuntimeImpl& operator=(const PlatformRuntimeImpl&) = delete;

  void UpdatePlatformRuntimeLibrary(const base::FilePath& library_path);

  // Returns the currently loaded library.
  // Callers must keep the returned scoped_refptr alive for the duration of
  // their operations to prevent the library from being unloaded
  // concurrently by another thread.
  scoped_refptr<PlatformRuntimeLibrary> GetLoadedLibrary();

 private:
  friend class base::NoDestructor<PlatformRuntimeImpl>;

  PlatformRuntimeImpl();
  ~PlatformRuntimeImpl();

  base::Lock lock_;
  scoped_refptr<PlatformRuntimeLibrary> loaded_library_ GUARDED_BY(lock_);
};

}  // namespace platform_runtime

#endif  // CHROME_COMMON_PLATFORM_RUNTIME_PLATFORM_RUNTIME_IMPL_H_
