// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_BRIDGE_H_
#define CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_BRIDGE_H_

#include "base/functional/callback.h"
#include "chrome/browser/webapps/installable/installed_webapp_provider.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/resolvers/permission_prompt_options.h"

class GURL;

namespace permissions {
struct PermissionPromptDecision;
}  // namespace permissions

class InstalledWebappBridge {
 public:
  using PermissionCallback = base::OnceCallback<void(
      const permissions::PermissionPromptDecision& decision)>;

  InstalledWebappBridge() = delete;
  InstalledWebappBridge(const InstalledWebappBridge&) = delete;
  InstalledWebappBridge& operator=(const InstalledWebappBridge&) = delete;

  static InstalledWebappProvider::RuleList GetInstalledWebappPermissions(
      ContentSettingsType type);

  static void SetProviderInstance(InstalledWebappProvider* provider);

  static void DecidePermission(ContentSettingsType type,
                               const GURL& origin_url,
                               const GURL& last_committed_url,
                               PermissionCallback callback);
};

#endif  // CHROME_BROWSER_WEBAPPS_INSTALLABLE_INSTALLED_WEBAPP_BRIDGE_H_
