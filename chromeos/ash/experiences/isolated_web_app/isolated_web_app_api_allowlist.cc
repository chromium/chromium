// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/isolated_web_app/isolated_web_app_api_allowlist.h"

#include "chromeos/constants/chromeos_features.h"
#include "components/webapps/isolated_web_apps/public/iwa_runtime_data_provider.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "url/origin.h"

namespace ash {

bool CanOriginAccessCrosIwaApi(const url::Origin& origin) {
  if (!chromeos::features::IsCrosIsolatedWebAppSetShapeAllowlistEnabled()) {
    return false;
  }

  if (origin.scheme() != webapps::kIsolatedAppScheme) {
    return false;
  }

  const auto* permissions = web_app::IwaRuntimeDataProvider::GetInstance()
                                .GetSpecialAppPermissionsInfo(origin.host());
  return permissions && permissions->allow_set_shape;
}

}  // namespace ash
