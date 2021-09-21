// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nonsfi/nonsfi_main.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "native_client/src/include/elf_auxv.h"
#include "native_client/src/public/nonsfi/elf_loader.h"
#include "ppapi/nacl_irt/irt_interfaces.h"

#if !defined(OS_NACL_NONSFI)
#error "nonsfi_main.cc must be built for nacl_helper_nonsfi."
#endif

namespace nacl {
namespace nonsfi {

namespace {

typedef void (*EntryPointType)(uintptr_t*);

// Default stack size of the plugin main thread. We heuristically chose 16M.
const size_t kStackSize = (16 << 20);

}  // namespace

class PluginMainDelegate : public base::PlatformThread::Delegate {
 public:
  explicit PluginMainDelegate(EntryPointType entry_point)
      : entry_point_(entry_point) {
  }

  ~PluginMainDelegate() override {}

  void ThreadMain() override {
    base::PlatformThread::SetName("NaClMainThread");

    // This will only happen once per process, so we give the permission to
    // create Singletons.
    base::PermanentSingletonAllowance::AllowSingleton();
    uintptr_t info[] = {
      0,  // Do not use fini.
      0,  // envc.
      0,  // argc.
      0,  // Null terminate for argv.
      0,  // Null terminate for envv.
      AT_SYSINFO,
      reinterpret_cast<uintptr_t>(&chrome_irt_query),
      AT_NULL,
      0,  // Null terminate for auxv.
    };
    entry_point_(info);
  }

 private:
  EntryPointType entry_point_;
};

void MainStart(int nexe_file) {
  EntryPointType entry_point =
      reinterpret_cast<EntryPointType>(NaClLoadElfFile(nexe_file));
  if (!base::PlatformThread::CreateNonJoinable(
          kStackSize, new PluginMainDelegate(entry_point))) {
    LOG(ERROR) << "LoadModuleRpc: Failed to create plugin main thread.";
    return;
  }
}

}  // namespace nonsfi
}  // namespace nacl
