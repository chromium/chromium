// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_resource_request_policy_delegate.h"

#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/url_constants.h"
#include "extensions/renderer/renderer_extension_registry.h"

namespace extensions {

ChromeResourceRequestPolicyDelegate::~ChromeResourceRequestPolicyDelegate() =
    default;

bool ChromeResourceRequestPolicyDelegate::
    ShouldAlwaysAllowRequestForFrameOrigin(const url::Origin& frame_origin) {
  return frame_origin.scheme() == chrome::kChromeSearchScheme;
}

bool ChromeResourceRequestPolicyDelegate::AllowLoadForDevToolsPage(
    const GURL& page_origin,
    const GURL& target_url) {
  if (!page_origin.SchemeIs(content::kChromeDevToolsScheme)) {
    return false;
  }

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(
          target_url, true /*include_guid*/);
  // Allow the load in the case of a non-existent extension. We'll just get a
  // 404 from the browser process.
  // TODO(devlin): Can this happen? Does devtools potentially make requests
  // to non-existent extensions?
  if (!extension) {
    return true;
  }

  // Devtools (chrome-extension:// URLs are loaded into frames of devtools to
  // support the devtools extension APIs).
  if (!chrome_manifest_urls::GetDevToolsPage(extension).is_empty()) {
    return true;
  }

  // Not an extension's devtools page.
  return false;
}

}  // namespace extensions
