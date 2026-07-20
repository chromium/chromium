// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GET_ISOLATED_WEB_APP_BROWSING_DATA_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GET_ISOLATED_WEB_APP_BROWSING_DATA_H_

#include <stdint.h>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"

class Profile;

namespace url {
class Origin;
}

namespace web_app {

// Calculates the total browsing data size for all installed Isolated Web Apps.
void GetIsolatedWebAppBrowsingData(
    Profile* profile,
    base::OnceCallback<void(base::flat_map<url::Origin, uint64_t>)> callback,
    const base::Location& call_location = FROM_HERE);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GET_ISOLATED_WEB_APP_BROWSING_DATA_H_
