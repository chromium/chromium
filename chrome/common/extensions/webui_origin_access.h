// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_WEBUI_ORIGIN_ACCESS_H_
#define CHROME_COMMON_EXTENSIONS_WEBUI_ORIGIN_ACCESS_H_

#include <string>
#include <vector>

#include "url/origin.h"

namespace extensions {

class Extension;

// How an extension context uses a chrome:// origin.
enum class WebUIOriginUse {
  // Subresource requests from extension frames, served by the chrome://
  // URLLoaderFactory.
  kFrameSubresource,
  // Requests from extension frames that ChildProcessSecurityPolicy checks
  // (ChildProcessSecurityPolicy::GrantRequestOrigin), e.g. subframe and other
  // renderer-initiated navigations.
  kFrameNavigation,
  // Requests from extension workers: served by the chrome:// worker
  // URLLoaderFactory and granted in ChildProcessSecurityPolicy (when
  // extensions_features::kComponentExtensionAllowWorkerChromeResources is
  // enabled).
  kWorker,
  // The origin is added, at the highest priority, to the extension's CORS
  // origin access allowlist in the renderer. Callers must only use this for
  // active extensions.
  //
  // This is a convenience for cross-origin fetches, not a security boundary:
  // the URLLoaderFactory and ChildProcessSecurityPolicy grants computed in the
  // browser are.
  kCorsAccess,

  kMinValue = kFrameSubresource,
  kMaxValue = kCorsAccess,
};

// Returns the chrome:// origins `extension` may use for `use`.
//
// This is the single source of truth for the WebUI origins granted by:
//   - ChromeContentBrowserClient (frame and worker URLLoaderFactories, and
//     worker ChildProcessSecurityPolicy),
//   - ChromeExtensionWebContentsObserver (frame ChildProcessSecurityPolicy),
//   - ChromeExtensionsClient::AddOriginAccessPermissions() (renderer CORS).
// New WebUI origins must be added to the table in the .cc file rather than at
// individual call sites. Two checks remain at the call sites: extensions with
// the "management" permission get CORS access to chrome://extension-icon/ (see
// ChromeExtensionsClient::AddOriginAccessPermissions()), and worker grants are
// gated on extensions_features::kComponentExtensionAllowWorkerChromeResources.
//
// Other layers still apply independently: whether a chrome:// host permission
// can be declared at all (CanSpecifyHostPermission() in permissions_parser.cc),
// whether CSP allows scripts from chrome://resources (csp_validator.cc), and
// whether the WebUI data source is registered.
std::vector<url::Origin> GetAllowedWebUIOrigins(const Extension& extension,
                                                WebUIOriginUse use);

// Returns the hosts of the chrome:// origins in GetAllowedWebUIOrigins(), for
// use as a chrome:// URLLoaderFactory's allowed hosts.
std::vector<std::string> GetAllowedChromeUIHosts(const Extension& extension,
                                                 WebUIOriginUse use);

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_WEBUI_ORIGIN_ACCESS_H_
