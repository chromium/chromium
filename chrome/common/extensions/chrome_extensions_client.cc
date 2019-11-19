// Copyright 2013 The Chromium Authors. All rights reserved.
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
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/chrome_extensions_api_provider.h"
#include "chrome/common/extensions/manifest_handlers/theme_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/core_extensions_api_provider.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/feature_channel.h"
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

// TODO(battre): Delete the HTTP URL once the blacklist is downloaded via HTTPS.
const char kExtensionBlocklistUrlPrefix[] =
    "http://www.gstatic.com/chrome/extensions/blacklist";
const char kExtensionBlocklistHttpsUrlPrefix[] =
    "https://www.gstatic.com/chrome/extensions/blacklist";

const char kThumbsWhiteListedExtension[] = "khopmbdjffemhegeeobelklnbglcdgfh";

}  // namespace

ChromeExtensionsClient::ChromeExtensionsClient() {
  AddAPIProvider(std::make_unique<ChromeExtensionsAPIProvider>());
  AddAPIProvider(std::make_unique<CoreExtensionsAPIProvider>());
}

ChromeExtensionsClient::~ChromeExtensionsClient() {
}

void ChromeExtensionsClient::Initialize() {
  // Set up the scripting whitelist.
  // Whitelist ChromeVox, an accessibility extension from Google that needs
  // the ability to script webui pages. This is temporary and is not
  // meant to be a general solution.
  // TODO(dmazzoni): remove this once we have an extension API that
  // allows any extension to request read-only access to webui pages.
  scripting_whitelist_.push_back(extension_misc::kChromeVoxExtensionId);
  InitializeWebStoreUrls(base::CommandLine::ForCurrentProcess());
}

void ChromeExtensionsClient::InitializeWebStoreUrls(
    base::CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kAppsGalleryURL)) {
    webstore_base_url_ =
        GURL(command_line->GetSwitchValueASCII(switches::kAppsGalleryURL));
  } else {
    webstore_base_url_ = GURL(extension_urls::kChromeWebstoreBaseURL);
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
  // When editing this function, be sure to add the same functionality to
  // FilterHostPermissions() above.
  for (auto i = hosts.begin(); i != hosts.end(); ++i) {
    // Filters out every URL pattern that matches chrome:// scheme.
    if (i->scheme() == content::kChromeUIScheme) {
      // chrome://favicon is the only URL for chrome:// scheme that we
      // want to support. We want to deprecate the "chrome" scheme.
      // We should not add any additional "host" here.
      if (GURL(chrome::kChromeUIFaviconURL).host() != i->host())
        continue;
      permissions->insert(APIPermission::kFavicon);
    } else {
      new_hosts->AddPattern(*i);
    }
  }
}

void ChromeExtensionsClient::SetScriptingWhitelist(
    const ExtensionsClient::ScriptingWhitelist& whitelist) {
  scripting_whitelist_ = whitelist;
}

const ExtensionsClient::ScriptingWhitelist&
ChromeExtensionsClient::GetScriptingWhitelist() const {
  return scripting_whitelist_;
}

URLPatternSet ChromeExtensionsClient::GetPermittedChromeSchemeHosts(
      const Extension* extension,
      const APIPermissionSet& api_permissions) const {
  URLPatternSet hosts;
  // Regular extensions are only allowed access to chrome://favicon.
  hosts.AddPattern(URLPattern(URLPattern::SCHEME_CHROMEUI,
                              chrome::kChromeUIFaviconURL));

  // Experimental extensions are also allowed chrome://thumb.
  //
  // TODO: A public API should be created for retrieving thumbnails.
  // See http://crbug.com/222856. A temporary hack is implemented here to
  // make chrome://thumbs available to NTP Russia extension as
  // non-experimental.
  if ((api_permissions.find(APIPermission::kExperimental) !=
       api_permissions.end()) ||
      (extension->id() == kThumbsWhiteListedExtension &&
       extension->from_webstore())) {
    hosts.AddPattern(URLPattern(URLPattern::SCHEME_CHROMEUI,
                                chrome::kChromeUIThumbnailURL));
  }
  return hosts;
}

bool ChromeExtensionsClient::IsScriptableURL(
    const GURL& url, std::string* error) const {
  // The gallery is special-cased as a restricted URL for scripting to prevent
  // access to special JS bindings we expose to the gallery (and avoid things
  // like extensions removing the "report abuse" link).
  // TODO(erikkay): This seems like the wrong test.  Shouldn't we we testing
  // against the store app extent?
  GURL store_url(extension_urls::GetWebstoreLaunchURL());
  if (url.DomainIs(store_url.host())) {
    if (error)
      *error = manifest_errors::kCannotScriptGallery;
    return false;
  }
  return true;
}

const GURL& ChromeExtensionsClient::GetWebstoreBaseURL() const {
  return webstore_base_url_;
}

const GURL& ChromeExtensionsClient::GetWebstoreUpdateURL() const {
  return webstore_update_url_;
}

bool ChromeExtensionsClient::IsBlacklistUpdateURL(const GURL& url) const {
  // The extension blacklist URL is returned from the update service and
  // therefore not determined by Chromium. If the location of the blacklist file
  // ever changes, we need to update this function. A DCHECK in the
  // ExtensionUpdater ensures that we notice a change. This is the full URL
  // of a blacklist:
  // http://www.gstatic.com/chrome/extensions/blacklist/l_0_0_0_7.txt
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
  const base::DictionaryValue* theme_images = ThemeInfo::GetImages(extension);
  if (theme_images) {
    for (base::DictionaryValue::Iterator it(*theme_images); !it.IsAtEnd();
         it.Advance()) {
      base::FilePath::StringType path;
      if (it.value().GetAsString(&path))
        image_paths.insert(base::FilePath(path));
    }
  }

  const ActionInfo* action = ActionInfo::GetAnyActionInfo(extension);
  if (action && !action->default_icon.empty())
    action->default_icon.GetPaths(&image_paths);

  return image_paths;
}

bool ChromeExtensionsClient::ExtensionAPIEnabledInExtensionServiceWorkers()
    const {
  return GetCurrentChannel() <=
         extension_misc::kMinChannelForServiceWorkerBasedExtension;
}

void ChromeExtensionsClient::AddOriginAccessPermissions(
    const Extension& extension,
    bool is_extension_active,
    std::vector<network::mojom::CorsOriginPatternPtr>* origin_patterns) const {
  // Allow component extensions to access chrome://theme/.
  //
  // We don't want to grant these permissions to inactive component extensions,
  // to avoid granting them in "unblessed" (non-extension) processes.  If a
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
  // whitelist entries need to be updated when the kManagement permission
  // changes.
  if (is_extension_active && extension.permissions_data()->HasAPIPermission(
                                 extensions::APIPermission::kManagement)) {
    origin_patterns->push_back(network::mojom::CorsOriginPattern::New(
        content::kChromeUIScheme, chrome::kChromeUIExtensionIconHost,
        /*port=*/0, network::mojom::CorsDomainMatchMode::kDisallowSubdomains,
        network::mojom::CorsPortMatchMode::kAllowAnyPort,
        network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority));
  }
}

}  // namespace extensions
