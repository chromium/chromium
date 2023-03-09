// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/browser_support.h"

#include "base/check_op.h"
#include "base/logging.h"

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
