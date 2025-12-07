// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_LIBRARY_HOLDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_LIBRARY_HOLDER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_native_library.h"
#include "base/types/pass_key.h"

namespace optimization_guide {

base::FilePath GetSharedLibraryPath();

// A OptimizationGuideLibraryHolder object encapsulates a reference to the
// PageContextEligibilityAPI shared library, exposing the library's API
// functions to callers and ensuring that the library remains loaded and usable
// throughout the object's lifetime.
class OptimizationGuideLibraryHolder {
 public:
  OptimizationGuideLibraryHolder(base::PassKey<OptimizationGuideLibraryHolder>,
                                 base::ScopedNativeLibrary library);
  ~OptimizationGuideLibraryHolder();

  OptimizationGuideLibraryHolder(const OptimizationGuideLibraryHolder& other) =
      delete;
  OptimizationGuideLibraryHolder& operator=(
      const OptimizationGuideLibraryHolder& other) = delete;

  OptimizationGuideLibraryHolder(OptimizationGuideLibraryHolder&& other) =
      default;
  OptimizationGuideLibraryHolder& operator=(
      OptimizationGuideLibraryHolder&& other) = default;

  // Creates an instance of OptimizationGuideLibraryHolder. May return nullopt
  // if the underlying library could not be loaded.
  static std::unique_ptr<OptimizationGuideLibraryHolder> Create();

  void* GetFunctionPointer(const char* function_name);

 private:
  base::ScopedNativeLibrary library_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_LIBRARY_HOLDER_H_
