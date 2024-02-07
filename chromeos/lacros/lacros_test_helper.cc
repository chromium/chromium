// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/lacros_test_helper.h"

#include "base/check.h"
#include "base/test/test_future.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/startup/browser_params_proxy.h"

namespace chromeos {
namespace {
base::Version GetAshVersion() {
  constexpr int min_mojo_version =
      crosapi::mojom::TestController::kGetAshVersionMinVersion;
  if (chromeos::LacrosService::Get()
          ->GetInterfaceVersion<crosapi::mojom::TestController>() <
      min_mojo_version) {
    return base::Version({0, 0, 0, 0});
  }

  base::test::TestFuture<const std::string&> future;
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::TestController>()
      ->GetAshVersion(future.GetCallback());
  return base::Version(future.Take());
}
}  // namespace

ScopedLacrosServiceTestHelper::ScopedLacrosServiceTestHelper() {
  CHECK(BrowserParamsProxy::IsCrosapiDisabledForTesting());
}

ScopedLacrosServiceTestHelper::~ScopedLacrosServiceTestHelper() = default;

bool IsAshVersionAtLeastForTesting(base::Version required_version) {
  DCHECK(required_version.IsValid());
  DCHECK(LacrosService::Get());
  static base::Version cached_ash_version = GetAshVersion();
  DCHECK(cached_ash_version.IsValid());
  return (cached_ash_version >= required_version);
}

}  // namespace chromeos
