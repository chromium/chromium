// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ASH_MIGRATIONS_ADOBE_EXPRESS_OEM_TO_DEFAULT_MIGRATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ASH_MIGRATIONS_ADOBE_EXPRESS_OEM_TO_DEFAULT_MIGRATION_H_

namespace web_app {

class WebAppSyncBridge;

namespace migrations {

// If the Adobe Express app is installed as an OEM install, migrate it instead
// to WebAppManagement::Type::kApsDefault. This is a migration away from a
// workaround added to install the app as WebAppManagement::Type::kOem before
// kApsDefault was supported. See b/300529104 for context.
// TODO(b/314865744): Remove this migration in ~M134.
void MigrateAdobeExpressFromOemInstallToDefault(WebAppSyncBridge* sync_bridge);

}  // namespace migrations

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ASH_MIGRATIONS_ADOBE_EXPRESS_OEM_TO_DEFAULT_MIGRATION_H_
