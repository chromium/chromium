// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/browser_support.h"

#include "base/check_op.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"

namespace ash::standalone_browser {
namespace {

BrowserSupport* g_instance = nullptr;
absl::optional<bool> g_cpu_supported_override_ = absl::nullopt;

}  // namespace

BrowserSupport::BrowserSupport() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

BrowserSupport::~BrowserSupport() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void BrowserSupport::Initialize() {
  // Currently, some tests rely on initializing ProfileManager a second time.
  // That causes this method to be called twice. Here, we take care of that
  // case by deallocating the old instance and allocating a new one.
  // TODO(andreaorru): remove the following code once there's no more tests
  // that rely on it.
  if (g_instance) {
    // We take metrics here to be sure that this code path is not used in
    // production, as it should only happen in tests.
    // TODO(andreaorru): remove the following code, once we're sure it's never
    // used in production.
    if (base::SysInfo::IsRunningOnChromeOS()) {
      base::UmaHistogramBoolean(
          "Ash.BrowserSupport.UnexpectedBrowserSupportInitialize", true);
      base::debug::DumpWithoutCrashing();
    }
    Shutdown();
  }

  // Calls the constructor, which in turn takes care of tracking the newly
  // created instance in `g_instance`, so that it's not leaked and can
  // later be destroyed via `Shutdown()`.
  new BrowserSupport();
}

// static
void BrowserSupport::Shutdown() {
  // Calls the destructor, which in turn takes care of setting `g_instance`
  // to NULL, to keep track of the state.
  delete g_instance;
}

// static
BrowserSupport* BrowserSupport::Get() {
  DCHECK(g_instance);
  return g_instance;
}

// static
bool BrowserSupport::IsCpuSupported() {
  if (g_cpu_supported_override_.has_value()) {
    return *g_cpu_supported_override_;
  }

#ifdef ARCH_CPU_X86_64
  // Some very old Flex devices are not capable to support the SSE4.2
  // instruction set. Those CPUs should not use Lacros as Lacros has only one
  // binary for all x86-64 platforms.
  return __builtin_cpu_supports("sse4.2");
#else
  return true;
#endif
}

void BrowserSupport::SetCpuSupportedForTesting(absl::optional<bool> value) {
  g_cpu_supported_override_ = value;
}

}  // namespace ash::standalone_browser
