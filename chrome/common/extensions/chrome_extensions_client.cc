// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/chrome_extensions_client.h"

#include <memory>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/common/chrome_resource_request_blocked_reason.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/chrome_extensions_api_provider.h"
#include "chrome/common/extensions/manifest_handlers/theme_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/constants.h"
#include "extensions/common/core_extensions_api_provider.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// TODO(battre): Delete the HTTP URL once the blocklist is downloaded via HTTPS.
const char kExtensionBlocklistUrlPrefix[] =
    "http://www.gstatic.com/chrome/extensions/blocklist";
const char kExtensionBlocklistHttpsUrlPrefix[] =
    "https://www.gstatic.com/chrome/extensions/blocklist";

}  // namespace

ChromeExtensionsClient::ChromeExtensionsClient() {
  AddAPIProvider(std::make_unique<ChromeExtensionsAPIProvider>());
  AddAPIProvider(std::make_unique<CoreExtensionsAPIProvider>());
}

ChromeExtensionsClient::~ChromeExtensionsClient() {
}

void ChromeExtensionsClient::Initialize() {
  // Set up the scripting allowlist.
  // Allowlist ChromeVox, an accessibility extension from Google that needs
  // the ability to script webui pages. This is temporary and is not
  // meant to be a general solution.
  // TODO(dmazzoni): remove this once we have an extension API that
  // allows any extension to request read-only access to webui pages.
  scripting_allowlist_.push_back(extension_misc::kChromeVoxExtensionId);
  InitializeWebStoreUrls(base::CommandLine::ForCurrentProcess());
}

void ChromeExtensionsClient::InitializeWebStoreUrls(
    base::CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kAppsGalleryURL)) {
    webstore_base_url_ =
        GURL(command_line->GetSwitchValueASCII(switches::kAppsGalleryURL));
    new_webstore_base_url_ =
        GURL(command_line->GetSwitchValueASCII(switches::kAppsGalleryURL));
  } else {
    webstore_base_url_ = GURL(extension_urls::kChromeWebstoreBaseURL);
    new_webstore_base_url_ = GURL(extension_urls::kNewChromeWebstoreBaseURL);
  }
  if (command_line->HasSwitch(switches::kAppsGalleryUpdateURL)) {
    webstore_update_url_ = GURL(
        command_line->GetSwitchValueASCII(switches::kAppsGalleryUpdateURL));
  } else {
    webstore_update_url_ = GURL(extension_urls::GetDefaultWebstoreUpdateUrl());
  }
}

const PermissionMessageProvider&
ChromeExtensionsClient::GetPermissionMessageProvider() const {
  return permission_message_provider_;
}

const std::string ChromeExtensionsClient::GetProductName() {
  return l10n_util::GetStringUTF8(IDS_PRODUCT_NAME);
}

void ChromeExtensionsClient::FilterHostPermissions(
    const URLPatternSet& hosts,
    URLPatternSet* new_hosts,
    PermissionIDSet* permissions) const {
  for (auto i = hosts.begin(); i != hosts.end(); ++i) {
    // Filters out every URL pattern that matches chrome:// scheme.
    if (i->scheme() == content::kChromeUIScheme) {
      // chrome://favicon is the only URL for chrome:// scheme that we
      // want to support. We want to deprecate the "chrome" scheme.
      // We should not add any additional "host" here.
      if (GURL(chrome::kChromeUIFaviconURL).host() != i->host())
        continue;
      permissions->insert(mojom::APIPermissionID::kFavicon);
    } else {
      new_hosts->AddPattern(*i);
    }
  }
}

void ChromeExtensionsClient::SetScriptingAllowlist(
    const ExtensionsClient::ScriptingAllowlist& allowlist) {
  scripting_allowlist_ = allowlist;
}

const ExtensionsClient::ScriptingAllowlist&
ChromeExtensionsClient::GetScriptingAllowlist() const {
  return scripting_allowlist_;
}

URLPatternSet ChromeExtensionsClient::GetPermittedChromeSchemeHosts(
      const Extension* extension,
      const APIPermissionSet& api_permissions) const {
  URLPatternSet hosts;

  // Do not allow any chrome-scheme hosts in MV3+ extensions.
  if (extension->manifest_version() >= 3)
    return hosts;

  // Regular extensions are only allowed access to chrome://favicon.
  hosts.AddPattern(URLPattern(URLPattern::SCHEME_CHROMEUI,
                              chrome::kChromeUIFaviconURL));

  return hosts;
}

bool ChromeExtensionsClient::IsScriptableURL(
    const GURL& url, std::string* error) const {
  // The gallery is special-cased as a restricted URL for scripting to prevent
  // access to special JS bindings we expose to the gallery (and avoid things
  // like extensions removing the "report abuse" link).
  if (extension_urls::IsWebstoreDomain(url)) {
    if (error)
      *error = manifest_errors::kCannotScriptGallery;
    return false;
  }
  return true;
}

const GURL& ChromeExtensionsClient::GetWebstoreBaseURL() const {
  return webstore_base_url_;
}

const GURL& ChromeExtensionsClient::GetNewWebstoreBaseURL() const {
  return new_webstore_base_url_;
}

const GURL& ChromeExtensionsClient::GetWebstoreUpdateURL() const {
  return webstore_update_url_;
}

bool ChromeExtensionsClient::IsBlocklistUpdateURL(const GURL& url) const {
  // The extension blocklist URL is returned from the update service and
  // therefore not determined by Chromium. If the location of the blocklist file
  // ever changes, we need to update this function. A DCHECK in the
  // ExtensionUpdater ensures that we notice a change. This is the full URL
  // of a blocklist:
  // http://www.gstatic.com/chrome/extensions/blocklist/l_0_0_0_7.txt
  return base::StartsWith(url.spec(), kExtensionBlocklistUrlPrefix,
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(url.spec(), kExtensionBlocklistHttpsUrlPrefix,
                          base::CompareCase::SENSITIVE);
}

std::set<base::FilePath> ChromeExtensionsClient::GetBrowserImagePaths(
    const Extension* extension) {
  std::set<base::FilePath> image_paths =
      ExtensionsClient::GetBrowserImagePaths(extension);

  // Theme images
  const base::Value::Dict* theme_images = ThemeInfo::GetImages(extension);
  if (theme_images) {
    for (const auto [key, value] : *theme_images) {
      if (value.is_string())
        image_paths.insert(base::FilePath::FromUTF8Unsafe(value.GetString()));
    }
  }

  const ActionInfo* action = ActionInfo::GetExtensionActionInfo(extension);
  if (action && !action->default_icon.empty())
    action->default_icon.GetPaths(&image_paths);

  return image_paths;
}

void ChromeExtensionsClient::AddOriginAccessPermissions(
    const Extension& extension,
    bool is_extension_active,
    std::vector<network::mojom::CorsOriginPatternPtr>* origin_patterns) const {
  // Allow component extensions to access chrome://theme/.
  //
  // We don't want to grant these permissions to inactive component extensions,
  // to avoid granting them in "unprivileged" (non-extension) processes.  If a
  // component extension somehow starts as inactive and becomes active later,
  // we'll re-init the origin permissions, so there's no danger in being
  // conservative. Components shouldn't be subject to enterprise policy controls
  // or blocking access to the webstore so they get the highest priority
  // allowlist entry.
  if (extensions::Manifest::IsComponentLocation(extension.location()) &&
      is_extension_active) {
    origin_patterns->push_back(network::mojom::CorsOriginPattern::New(
        content::kChromeUIScheme, chrome::kChromeUIThemeHost, /*port=*/0,
        network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
        network::mojom::CorsPortMatchMode::kAllowAnyPort,
        network::mojom::CorsOriginAccessMatchPriority::kMaxPriority));
  }

  // TODO(jstritar): We should try to remove this special case. Also, these
  // allowed entries need to be updated when the kManagement permission
  // changes.
  if (is_extension_active && extension.permissions_data()->HasAPIPermission(
                                 mojom::APIPermissionID::kManagement)) {
    origin_patterns->push_back(network::mojom::CorsOriginPattern::New(
        content::kChromeUIScheme, chrome::kChromeUIExtensionIconHost,
        /*port=*/0, network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
        network::mojom::CorsPortMatchMode::kAllowAnyPort,
        network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority));
  }
}

std::optional<int> ChromeExtensionsClient::GetExtensionExtendedErrorCode()
    const {
  return static_cast<int>(ChromeResourceRequestBlockedReason::kExtension);
}

}  // namespace extensions
