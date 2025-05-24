// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_library_holder.h"

#include <memory>
#include <optional>

#include "base/base_paths.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/types/pass_key.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#endif

namespace optimization_guide {

namespace {
constexpr std::string_view kSharedLibraryName = "optimization_guide_internal";
}

base::FilePath GetSharedLibraryPath() {
  base::FilePath base_dir;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(IS_MAC)
  if (base::apple::AmIBundled()) {
    base_dir = base::apple::FrameworkBundlePath().Append("Libraries");
  } else {
#endif  // BUILDFLAG(IS_MAC)
    CHECK(base::PathService::Get(base::DIR_MODULE, &base_dir));
#if BUILDFLAG(IS_MAC)
  }
#endif  // BUILDFLAG(IS_MAC)
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) &&
        // !BUILDFLAG(IS_FUCHSIA)

  return base_dir.AppendASCII(
      base::GetNativeLibraryName(std::string(kSharedLibraryName)));
}

OptimizationGuideLibraryHolder::OptimizationGuideLibraryHolder(
    base::PassKey<OptimizationGuideLibraryHolder>,
    base::ScopedNativeLibrary library)
    : library_(std::move(library)) {}

OptimizationGuideLibraryHolder::~OptimizationGuideLibraryHolder() = default;

// static
DISABLE_CFI_DLSYM
std::unique_ptr<OptimizationGuideLibraryHolder>
OptimizationGuideLibraryHolder::Create() {
  base::NativeLibraryLoadError error;
  base::NativeLibrary library =
      base::LoadNativeLibrary(GetSharedLibraryPath(), &error);
  if (!library) {
    return {};
  }

  base::ScopedNativeLibrary scoped_library(library);
  return std::make_unique<OptimizationGuideLibraryHolder>(
      base::PassKey<OptimizationGuideLibraryHolder>(),
      std::move(scoped_library));
}

void* OptimizationGuideLibraryHolder::GetFunctionPointer(
    const char* function_name) {
  return library_.GetFunctionPointer(function_name);
}

}  // namespace optimization_guide
