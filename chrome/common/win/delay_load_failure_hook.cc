// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// windows.h needs to be included before delayimp.h.
#include <windows.h>

#include <delayimp.h>

#include <algorithm>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/common/win/delay_load_failure_support.h"

namespace {

// Delay load failure hook that generates a crash report. By default a failure
// to delay load will trigger an exception handled by the delay load runtime and
// this won't generate a crash report.
FARPROC WINAPI DelayLoadFailureHook(unsigned reason, DelayLoadInfo* dll_info) {
  std::string_view raw_dll_name(dll_info->szDll);
  if (raw_dll_name.size() < MAX_PATH) {
    // The following modules are speculatively loaded by their consumers via
    // base::win::LoadAllImportsForDll, which installs an exception handler to
    // intercept failures and return an error to the caller. These consumers
    // gracefully handle such failures, so return 0 to tell the runtime that
    // this failure hasn't been handled so that the exception is raised.
    // TODO(crbug.com/536936869): Remove this list.
    static constexpr auto kOptionalModules =
        base::MakeFixedFlatSet<std::string_view>(
            {"bthprops.cpl", "mf.dll", "mfplat.dll", "mfreadwrite.dll",
             "tbs.dll"});
    // Copy and transform the module name into lower case.
    char dll_name_buffer[MAX_PATH];
    base::span<char> dll_name_buffer_span(dll_name_buffer);
    auto out_end =
        std::ranges::transform(raw_dll_name, dll_name_buffer_span.begin(),
                               [](char c) { return base::ToLowerASCII(c); })
            .out;
    auto len =
        base::checked_cast<size_t>(out_end - dll_name_buffer_span.begin());
    if (kOptionalModules.contains({&dll_name_buffer_span.front(), len})) {
      return 0;
    }
  }

  return HandleDelayLoadFailureCommon(reason, dll_info);
}

}  // namespace

// Set the delay load failure hook to the function above.
//
// The |__pfnDliFailureHook2| failure notification hook gets called
// automatically by the delay load runtime in case of failure, see
// https://docs.microsoft.com/en-us/cpp/build/reference/failure-hooks?view=vs-2019
// for more information about this.
extern "C" const PfnDliHook __pfnDliFailureHook2 = DelayLoadFailureHook;
