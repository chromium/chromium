// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_ISOLATED_WEB_APP_API_ALLOWLIST_H_
#define CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_ISOLATED_WEB_APP_API_ALLOWLIST_H_

namespace url {
class Origin;
}

namespace ash {

// Returns true if the given origin is allowed to access the CrOS IWA API.
bool CanOriginAccessCrosIwaApi(const url::Origin& origin);

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_ISOLATED_WEB_APP_API_ALLOWLIST_H_
