// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_CAST_EXTENSIONS_CLIENT_H_
#define CHROMECAST_COMMON_CAST_EXTENSIONS_CLIENT_H_

#include "base/compiler_specific.h"
#include "extensions/common/extensions_client.h"
#include "url/gurl.h"

namespace extensions {

// The cast_shell implementation of ExtensionsClient.
class CastExtensionsClient : public ExtensionsClient {
 public:
  CastExtensionsClient();

  CastExtensionsClient(const CastExtensionsClient&) = delete;
  CastExtensionsClient& operator=(const CastExtensionsClient&) = delete;

  ~CastExtensionsClient() override;

  // ExtensionsClient overrides:
  void Initialize() override;
  void InitializeWebStoreUrls(base::CommandLine* command_line) override;
  const PermissionMessageProvider& GetPermissionMessageProvider()
      const override;
  const std::string GetProductName() override;
  void FilterHostPermissions(const URLPatternSet& hosts,
                             URLPatternSet* new_hosts,
                             PermissionIDSet* permissions) const override;
  void SetScriptingAllowlist(const ScriptingAllowlist& allowlist) override;
  const ScriptingAllowlist& GetScriptingAllowlist() const override;
  URLPatternSet GetPermittedChromeSchemeHosts(
      const Extension* extension,
      const APIPermissionSet& api_permissions) const override;
  bool IsScriptableURL(const GURL& url, std::string* error) const override;
  const GURL& GetWebstoreBaseURL() const override;
  const GURL& GetWebstoreUpdateURL() const override;
  bool IsBlocklistUpdateURL(const GURL& url) const override;

 private:
  ScriptingAllowlist scripting_allowlist_;

  const GURL webstore_base_url_;
  const GURL webstore_update_url_;
};

}  // namespace extensions

#endif  // CHROMECAST_COMMON_CAST_EXTENSIONS_CLIENT_H_
