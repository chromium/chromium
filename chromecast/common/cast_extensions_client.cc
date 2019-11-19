// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cast_extensions_client.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "chromecast/common/cast_extensions_api_provider.h"
#include "extensions/common/api/api_features.h"
#include "extensions/common/api/behavior_features.h"
#include "extensions/common/api/generated_schemas.h"
#include "extensions/common/api/manifest_features.h"
#include "extensions/common/api/permission_features.h"
#include "extensions/common/core_extensions_api_provider.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/manifest_feature.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

namespace {

// TODO(jamescook): Refactor ChromePermissionsMessageProvider so we can share
// code. For now, this implementation does nothing.
class ShellPermissionMessageProvider : public PermissionMessageProvider {
 public:
  ShellPermissionMessageProvider() {}
  ~ShellPermissionMessageProvider() override {}

  // PermissionMessageProvider implementation.
  PermissionMessages GetPermissionMessages(
      const PermissionIDSet& permissions) const override {
    return PermissionMessages();
  }

  PermissionMessages GetPowerfulPermissionMessages(
      const PermissionIDSet& permissions) const override {
    return PermissionMessages();
  }

  bool IsPrivilegeIncrease(const PermissionSet& granted_permissions,
                           const PermissionSet& requested_permissions,
                           Manifest::Type extension_type) const override {
    // Ensure we implement this before shipping.
    CHECK(false);
    return false;
  }

  PermissionIDSet GetAllPermissionIDs(
      const PermissionSet& permissions,
      Manifest::Type extension_type) const override {
    return PermissionIDSet();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellPermissionMessageProvider);
};

}  // namespace

CastExtensionsClient::CastExtensionsClient()
    : webstore_base_url_(extension_urls::kChromeWebstoreBaseURL),
      webstore_update_url_(extension_urls::kChromeWebstoreUpdateURL) {
  AddAPIProvider(std::make_unique<CoreExtensionsAPIProvider>());
  AddAPIProvider(std::make_unique<CastExtensionsAPIProvider>());
}

CastExtensionsClient::~CastExtensionsClient() {}

void CastExtensionsClient::Initialize() {
  // TODO(jamescook): Do we need to whitelist any extensions?
}

void CastExtensionsClient::InitializeWebStoreUrls(
    base::CommandLine* command_line) {}

const PermissionMessageProvider&
CastExtensionsClient::GetPermissionMessageProvider() const {
  NOTIMPLEMENTED();
  static base::NoDestructor<ShellPermissionMessageProvider>
      g_permission_message_provider;
  return *g_permission_message_provider;
}

const std::string CastExtensionsClient::GetProductName() {
  return "cast_shell";
}

void CastExtensionsClient::FilterHostPermissions(
    const URLPatternSet& hosts,
    URLPatternSet* new_hosts,
    PermissionIDSet* permissions) const {
  NOTIMPLEMENTED();
}

void CastExtensionsClient::SetScriptingWhitelist(
    const ScriptingWhitelist& whitelist) {
  scripting_whitelist_ = whitelist;
}

const ExtensionsClient::ScriptingWhitelist&
CastExtensionsClient::GetScriptingWhitelist() const {
  // TODO(jamescook): Real whitelist.
  return scripting_whitelist_;
}

URLPatternSet CastExtensionsClient::GetPermittedChromeSchemeHosts(
    const Extension* extension,
    const APIPermissionSet& api_permissions) const {
  NOTIMPLEMENTED();
  return URLPatternSet();
}

bool CastExtensionsClient::IsScriptableURL(const GURL& url,
                                           std::string* error) const {
  NOTIMPLEMENTED();
  return true;
}

const GURL& CastExtensionsClient::GetWebstoreBaseURL() const {
  return webstore_base_url_;
}

const GURL& CastExtensionsClient::GetWebstoreUpdateURL() const {
  return webstore_update_url_;
}

bool CastExtensionsClient::IsBlacklistUpdateURL(const GURL& url) const {
  return true;
}

}  // namespace extensions
