// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"

#include <ostream>
#include <string>
#include <vector>

#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {
namespace {

struct OsIntegrationTestOverrideState {
  base::Lock lock;
  scoped_refptr<OsIntegrationTestOverride> global_os_integration_test_override
      GUARDED_BY(lock);
};

OsIntegrationTestOverrideState&
GetMutableOsIntegrationTestOverrideStateForTesting() {
  static base::NoDestructor<OsIntegrationTestOverrideState>
      g_os_integration_test_override;
  return *g_os_integration_test_override.get();
}
}  // namespace

// static
const scoped_refptr<OsIntegrationTestOverride>&
OsIntegrationTestOverride::Get() {
  auto& state = GetMutableOsIntegrationTestOverrideStateForTesting();
  base::AutoLock state_lock(state.lock);
  return state.global_os_integration_test_override;
}

OsIntegrationTestOverride::OsIntegrationTestOverride() = default;
OsIntegrationTestOverride::~OsIntegrationTestOverride() = default;

OsIntegrationTestOverrideImpl*
OsIntegrationTestOverride::AsOsIntegrationTestOverrideImpl() {
  CHECK_IS_TEST();
  return nullptr;
}

// static
void OsIntegrationTestOverride::SetForTesting(
    scoped_refptr<OsIntegrationTestOverride> override) {
  CHECK_IS_TEST();
  auto& state = GetMutableOsIntegrationTestOverrideStateForTesting();
  base::AutoLock state_lock(state.lock);
  CHECK(!(override && state.global_os_integration_test_override))
      << "Cannot set a test override when one already exists";
  state.global_os_integration_test_override = std::move(override);
}

}  // namespace web_app
