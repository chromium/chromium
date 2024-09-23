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
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {
namespace {

struct OsIntegrationTestOverrideState {
  base::Lock lock;
  scoped_refptr<OsIntegrationTestOverride> global_os_integration_test_override
      GUARDED_BY(lock);
  int blocking_registration_count GUARDED_BY(lock) = 0;
};

OsIntegrationTestOverrideState&
GetMutableOsIntegrationTestOverrideStateForTesting() {
  static base::NoDestructor<OsIntegrationTestOverrideState>
      g_os_integration_test_override;
  return *g_os_integration_test_override.get();
}
}  // namespace

// static
void OsIntegrationTestOverride::CheckOsIntegrationAllowed() {
#if !BUILDFLAG(IS_CHROMEOS)
  // Note: Using OsIntegrationManager::SuppressForTesting disables os
  // integration, even if OsIntegrationTestOverride is specified. In this case,
  // os integration is still not allowed, and anything needing it (like
  // launching) should call this function & check-fail here.
  bool os_integration_can_occur_in_tests =
      !OsIntegrationManager::AreOsHooksSuppressedForTesting() &&
      ::web_app::OsIntegrationTestOverride::Get();
  if (!os_integration_can_occur_in_tests) {
    CHECK_IS_NOT_TEST()
        << "Please initialize an "
           "`OsIntegrationTestOverrideBlockingRegistration`"
           "to allow fully installed web apps with OS integration in tests. In "
           "unit tests it may be required to call "
           "`FakeWebAppProvider::UseRealOsIntegrationManager()` during set up.";
  }
#endif
}

// static
scoped_refptr<OsIntegrationTestOverride> OsIntegrationTestOverride::Get() {
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
scoped_refptr<OsIntegrationTestOverride>
OsIntegrationTestOverride::GetOrCreateForBlockingRegistration(
    base::FunctionRef<scoped_refptr<OsIntegrationTestOverride>()>
        creation_function) {
  CHECK_IS_TEST();
  auto& state = GetMutableOsIntegrationTestOverrideStateForTesting();
  base::AutoLock state_lock(state.lock);
  state.blocking_registration_count += 1;
  if (state.global_os_integration_test_override) {
    return state.global_os_integration_test_override;
  }
  scoped_refptr<OsIntegrationTestOverride> integration_override =
      creation_function();
  CHECK(integration_override);
  state.global_os_integration_test_override = std::move(integration_override);
  return state.global_os_integration_test_override;
}

// static
bool OsIntegrationTestOverride::DecreaseBlockingRegistrationCountMaybeReset() {
  CHECK_IS_TEST();
  auto& state = GetMutableOsIntegrationTestOverrideStateForTesting();
  base::AutoLock state_lock(state.lock);
  state.blocking_registration_count -= 1;
  CHECK_GE(state.blocking_registration_count, 0);
  if (state.blocking_registration_count == 0) {
    state.global_os_integration_test_override.reset();
    return true;
  }
  return false;
}

}  // namespace web_app
