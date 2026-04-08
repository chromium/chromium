// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/detect_inappropriate_exit.h"

#include <dlfcn.h>
#include <execinfo.h>
#include <mach-o/loader.h>
#include <stdlib.h>

#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"

namespace chrome {

namespace {

// A C++ adaptation of <execinfo.h>’s `backtrace`.
std::vector<const void*> Backtrace(const int max_frames = 64) {
  std::vector<const void*> return_addresses(max_frames);

  // Use `const_cast` to adapt to `backtrace`’s prototype, while leaving this
  // function’s exposed interface conveying `std::vector<const void*>`. This
  // allows better type safety: the return addresses are pointers to program
  // text, which should not be modified. `backtrace` does not actually write to
  // the (here, null) pointers in the vector it’s provided—they’re just
  // uninitialized storage from its perspective.
  const int size = backtrace(const_cast<void**>(return_addresses.data()),
                             return_addresses.size());

  return_addresses.resize(size);

  return return_addresses;
}

// A C++ adaptation of <dlfcn.h>’s `Dl_info`.
struct DlInfo {
  explicit DlInfo(const Dl_info& dl_info)
      : module_path(dl_info.dli_fname),
        module_base_address(
            reinterpret_cast<mach_header_64*>(dl_info.dli_fbase)),
        symbol(dl_info.dli_sname ? std::optional<std::string>(dl_info.dli_sname)
                                 : std::optional<std::string>()),
        symbol_address(dl_info.dli_saddr) {}

  std::string module_path;
  raw_ptr<const mach_header_64> module_base_address;
  std::optional<std::string> symbol;
  raw_ptr<const void> symbol_address;
};

// A C++ adaptation of <dlfcn.h>’s `dladdr`.
std::optional<DlInfo> DlAddr(const void* const address) {
  Dl_info dl_info;
  if (!dladdr(address, &dl_info)) {
    return std::nullopt;
  }

  return DlInfo(dl_info);
}

// Determines the non-libc caller of libc’s `exit` on the call stack. This
// obviously only makes sense from a callee of `exit`, so it’s intended to be
// used from an `atexit` handler.
//
// Returns a `DlInfo` describing the non-libc caller of libc `exit`. If libc’s
// `exit` or its non-libc caller are not found within a reasonable number of
// stack frames, returns `std::nullopt`.
std::optional<DlInfo> ExitCaller() {
  static constexpr char kLibCPath[] = "/usr/lib/system/libsystem_c.dylib";

  bool found_exit = false;
  for (const void* const return_address : Backtrace()) {
    const std::optional<DlInfo> dl_info = DlAddr(return_address);
    if (!dl_info.has_value()) {
      continue;
    }

    if (!found_exit) {
      if (dl_info->module_path == kLibCPath &&
          (dl_info->symbol.has_value() && dl_info->symbol == "exit")) {
        found_exit = true;
      }
    } else if (dl_info->module_path != kLibCPath) {
      return dl_info;
    }
  }

  return std::nullopt;
}

// Crashes via a `CHECK` failure. This is intended to be used when an
// inappropriate exit is detected. It’s broken out into a separate function to
// provide a clear and unambiguous indication of the crash reason in symbolized
// backtraces.
[[noreturn]] void ExitSixtyNineDetected() {
  CHECK(false);
}

// *69 for exit(69).
//
// Determines whether an inappropriate call to `exit` has been made, crashing if
// so. If the `exit` is appropriate, its propriety cannot be determined, or no
// `exit` call has been made, this function does nothing. This is intended to be
// used as an `atexit` handler.
//
// The implementation does not have access to the `status` argument passed to
// `exit`, but it can do a backtrace. The technique is to look for any `exit`
// call that comes from the OS’ ViewBridge module, which is where the
// problematic `exit(69)` calls are thought to be originating.
void DetectExitSixtyNineAtExit() {
  const std::optional<DlInfo> exit_caller = ExitCaller();
  if (exit_caller.has_value() &&
      exit_caller->module_path ==
          "/System/Library/PrivateFrameworks/ViewBridge.framework/Versions/A/"
          "ViewBridge") {
    [[clang::noinline]] ExitSixtyNineDetected();
  }
}

}  // namespace

void InitializeExitSixtyNineDetector() {
  atexit(DetectExitSixtyNineAtExit);
}

}  // namespace chrome
