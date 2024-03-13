// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_REMOVE_ISOLATED_WEB_APP_DATA_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_REMOVE_ISOLATED_WEB_APP_DATA_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/web_app.h"

class Profile;

namespace url {
class Origin;
}  // namespace url

namespace web_app {

class IsolatedWebAppStorageLocation;

// Removes all browsing data associated with the Isolated Web App with the
// given origin.
void RemoveIsolatedWebAppBrowsingData(Profile* profile,
                                      const url::Origin& iwa_origin,
                                      base::OnceClosure callback);

// Closes the reader (if any) that use the singed web bundle in the |location|
// and deletes the location's directory if it is owned by Chrome.
void CloseAndDeleteBundle(Profile* profile,
                          const IsolatedWebAppStorageLocation& location,
                          base::OnceClosure callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_REMOVE_ISOLATED_WEB_APP_DATA_H_
