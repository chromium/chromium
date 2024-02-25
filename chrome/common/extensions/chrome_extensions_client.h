// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_CHROME_EXTENSIONS_CLIENT_H_
#define CHROME_COMMON_EXTENSIONS_CHROME_EXTENSIONS_CLIENT_H_

#include <memory>

#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "extensions/common/extensions_client.h"
#include "url/gurl.h"

namespace extensions {

// The implementation of ExtensionsClient for Chrome, which encapsulates the
// global knowledge of features, permissions, and manifest fields.
class ChromeExtensionsClient : public ExtensionsClient {
 public:
  ChromeExtensionsClient();

  ChromeExtensionsClient(const ChromeExtensionsClient&) = delete;
  ChromeExtensionsClient& operator=(const ChromeExtensionsClient&) = delete;

  ~ChromeExtensionsClient() override;

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
  const GURL& GetNewWebstoreBaseURL() const override;
  const GURL& GetWebstoreUpdateURL() const override;
  bool IsBlocklistUpdateURL(const GURL& url) const override;
  std::set<base::FilePath> GetBrowserImagePaths(
      const Extension* extension) override;
  void AddOriginAccessPermissions(
      const Extension& extension,
      bool is_extension_active,
      std::vector<network::mojom::CorsOriginPatternPtr>* origin_patterns)
      const override;
  std::optional<int> GetExtensionExtendedErrorCode() const override;

 private:
  const ChromePermissionMessageProvider permission_message_provider_;

  // An allowlist of extensions that can script anywhere. Do not add to this
  // list (except in tests) without consulting the Extensions team first.
  // Note: Component extensions have this right implicitly and do not need to be
  // added to this list.
  ScriptingAllowlist scripting_allowlist_;

  GURL webstore_base_url_;
  GURL new_webstore_base_url_;
  GURL webstore_update_url_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_CHROME_EXTENSIONS_CLIENT_H_
