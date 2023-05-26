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
base::AutoReset<bool> BrowserSupport::SetLacrosEnabledForTest(
    bool force_enabled) {
  return base::AutoReset<bool>(&lacros_enabled_for_test_, force_enabled);
}

// static
bool BrowserSupport::GetLacrosEnabledForTest() {
  return lacros_enabled_for_test_;
}

// static
bool BrowserSupport::lacros_enabled_for_test_ = false;

}  // namespace ash::standalone_browser
