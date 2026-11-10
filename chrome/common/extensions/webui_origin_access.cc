// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/webui_origin_access.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/enum_set.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/csp_validator.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using enum WebUIOriginUse;
using WebUIOriginUses = base::EnumSet<WebUIOriginUse,
                                      WebUIOriginUse::kMinValue,
                                      WebUIOriginUse::kMaxValue>;

// Which extensions an entry applies to.
enum class Audience {
  // Component extensions and platform apps.
  kComponentExtensionsAndPlatformApps,
  // Component extensions only (not platform apps).
  kComponentExtensions,
  // Component extensions of any type, including component hosted apps. Only
  // used for CORS entries, which are keyed to the extension's origin.
  kComponentAnyType,
  // Extensions, legacy packaged apps, and component platform apps. Hosted apps
  // are excluded because they are served via web servers (and are generally
  // never given access to Chrome APIs).
  kExtensionsAndApps,
};
using enum Audience;

// Additional condition an extension in the audience must meet.
enum class Gate {
  kNone,
  // The extension must declare the origin in its "host_permissions". Whether
  // it can declare it at all is decided separately by
  // CanSpecifyHostPermission() in permissions_parser.cc (e.g. only
  // kComponent, not kExternalComponent, extensions may declare chrome:// host
  // permissions by default).
  kHostPermission,
  // Test-only: requires --allow-webui-test-in-component-extensions (and a
  // kComponent location). See csp_validator::IsWebUITestAllowedForLocation().
  kWebUITestSwitch,
};
using enum Gate;

struct WebUIOriginGrant {
  WebUIOriginUses uses;
  std::string_view host;
  Audience audience;
  Gate gate = Gate::kNone;
};

// The complete set of WebUI origins extensions may access.
constexpr WebUIOriginGrant kWebUIOriginGrants[] = {
    // Shared WebUI resources for component extensions and platform apps.
    {.uses = {kFrameSubresource, kFrameNavigation},
     .host = content::kChromeUIResourcesHost,
     .audience = kComponentExtensionsAndPlatformApps},
    {.uses = {kFrameSubresource, kFrameNavigation},
     .host = chrome::kChromeUIThemeHost,
     .audience = kComponentExtensionsAndPlatformApps},
    // Workers of platform apps don't get chrome://resources.
    {.uses = {kWorker},
     .host = content::kChromeUIResourcesHost,
     .audience = kComponentExtensions},
    // CORS access is broader than frame access: it applies to component
    // extensions of any type.
    {.uses = {kCorsAccess},
     .host = chrome::kChromeUIThemeHost,
     .audience = kComponentAnyType},

    // Test-only WebUI resources. chrome://webui-test only exists in browser
    // tests. Its helpers import from chrome://resources, so that host is
    // allowed for CORS alongside it.
    {.uses = {kFrameSubresource},
     .host = chrome::kChromeUIWebUITestHost,
     .audience = kComponentExtensionsAndPlatformApps,
     .gate = kWebUITestSwitch},
    {.uses = {kCorsAccess},
     .host = chrome::kChromeUIWebUITestHost,
     .audience = kComponentAnyType,
     .gate = kWebUITestSwitch},
    {.uses = {kCorsAccess},
     .host = content::kChromeUIResourcesHost,
     .audience = kComponentAnyType,
     .gate = kWebUITestSwitch},

    // Opt-in via "host_permissions".
    {.uses = {kFrameSubresource},
     .host = chrome::kChromeUIImageHost,
     .audience = kComponentExtensionsAndPlatformApps,
     .gate = kHostPermission},

    // Icons for extensions and apps.
    {.uses = {kFrameSubresource, kFrameNavigation},
     .host = chrome::kChromeUIFaviconHost,
     .audience = kExtensionsAndApps},
    {.uses = {kFrameSubresource, kFrameNavigation},
     .host = chrome::kChromeUIExtensionIconHost,
     .audience = kExtensionsAndApps},
    {.uses = {kFrameSubresource},
     .host = chrome::kChromeUIAppIconHost,
     .audience = kExtensionsAndApps},
};

// Component hosted apps only get CORS entries.
static_assert(std::ranges::none_of(kWebUIOriginGrants, [](const auto& grant) {
  return grant.audience == kComponentAnyType &&
         grant.uses.HasAny({kFrameSubresource, kFrameNavigation, kWorker});
}));

url::Origin ToOrigin(const WebUIOriginGrant& grant) {
  return url::Origin::CreateFromNormalizedTuple(
      content::kChromeUIScheme, std::string(grant.host), /*port=*/0);
}

bool IsInAudience(const Extension& extension, Audience audience) {
  const bool is_component = Manifest::IsComponentLocation(extension.location());
  const bool is_extension_or_platform_app =
      extension.is_extension() || extension.is_platform_app();
  switch (audience) {
    case kComponentExtensionsAndPlatformApps:
      return is_extension_or_platform_app && is_component;
    case kComponentExtensions:
      return extension.is_extension() && is_component;
    case kComponentAnyType:
      return is_component;
    case kExtensionsAndApps:
      return extension.is_extension() || extension.is_legacy_packaged_app() ||
             (extension.is_platform_app() && is_component);
  }
}

bool PassesGate(const Extension& extension, const WebUIOriginGrant& grant) {
  switch (grant.gate) {
    case kNone:
      return true;
    case kHostPermission:
      return extension.permissions_data()->HasHostPermission(
          ToOrigin(grant).GetURL());
    case kWebUITestSwitch:
      return csp_validator::IsWebUITestAllowedForLocation(extension.location());
  }
}

}  // namespace

std::vector<url::Origin> GetAllowedWebUIOrigins(const Extension& extension,
                                                WebUIOriginUse use) {
  std::vector<url::Origin> origins;
  for (const WebUIOriginGrant& grant : kWebUIOriginGrants) {
    if (grant.uses.Has(use) && IsInAudience(extension, grant.audience) &&
        PassesGate(extension, grant)) {
      origins.push_back(ToOrigin(grant));
    }
  }
  return origins;
}

std::vector<std::string> GetAllowedChromeUIHosts(const Extension& extension,
                                                 WebUIOriginUse use) {
  std::vector<std::string> hosts;
  for (const url::Origin& origin : GetAllowedWebUIOrigins(extension, use)) {
    hosts.push_back(origin.host());
  }
  return hosts;
}

}  // namespace extensions
