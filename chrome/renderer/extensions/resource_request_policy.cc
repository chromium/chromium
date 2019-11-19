// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/resource_request_policy.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

ResourceRequestPolicy::ResourceRequestPolicy(Dispatcher* dispatcher)
    : dispatcher_(dispatcher) {}
ResourceRequestPolicy::~ResourceRequestPolicy() = default;

void ResourceRequestPolicy::OnExtensionLoaded(const Extension& extension) {
  if (WebAccessibleResourcesInfo::HasWebAccessibleResources(&extension) ||
      WebviewInfo::HasWebviewAccessibleResources(
          extension, dispatcher_->webview_partition_id()) ||
      // Hosted app icons are accessible.
      // TODO(devlin): Should we incorporate this into
      // WebAccessibleResourcesInfo?
      (extension.is_hosted_app() && !IconsInfo::GetIcons(&extension).empty())) {
    web_accessible_ids_.insert(extension.id());
  }
}

void ResourceRequestPolicy::OnExtensionUnloaded(
    const ExtensionId& extension_id) {
  web_accessible_ids_.erase(extension_id);
}

// This method does a security check whether chrome-extension:// URLs can be
// requested by the renderer. Since this is in an untrusted process, the browser
// has a similar check to enforce the policy, in case this process is exploited.
// If you are changing this function, ensure equivalent checks are added to
// extension_protocols.cc's AllowExtensionResourceLoad.
bool ResourceRequestPolicy::CanRequestResource(
    const GURL& resource_url,
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type) {
  CHECK(resource_url.SchemeIs(kExtensionScheme));

  GURL frame_url = frame->GetDocument().Url();
  url::Origin frame_origin = frame->GetDocument().GetSecurityOrigin();

  // Navigations from chrome://, devtools:// or chrome-search:// pages need to
  // be allowed, even if the target |url| is not web-accessible.  See also:
  // - https://crbug.com/662602
  // - similar scheme checks in ExtensionNavigationThrottle
  if (frame_origin.scheme() == content::kChromeUIScheme ||
      frame_origin.scheme() == content::kChromeDevToolsScheme ||
      frame_origin.scheme() == chrome::kChromeSearchScheme) {
    return true;
  }

  // The page_origin may be GURL("null") for unique origins like data URLs,
  // but this is ok for the checks below.  We only care if it matches the
  // current extension or has a devtools scheme.
  GURL page_origin = url::Origin(frame->Top()->GetSecurityOrigin()).GetURL();

  GURL extension_origin = resource_url.GetOrigin();

  // We always allow loads in the following cases, regardless of web accessible
  // resources:

  // Empty urls (needed for some edge cases when we have empty urls).
  if (frame_url.is_empty())
    return true;

  // Extensions requesting their own resources (frame_url check is for images,
  // page_url check is for iframes).
  // TODO(devlin): We should be checking the ancestor chain, not just the
  // top-level frame. Additionally, we should be checking the security origin
  // of the frame, to account for about:blank subframes being scripted by an
  // extension parent (though we'll still need the frame origin check for
  // sandboxed frames).
  if (frame_url.GetOrigin() == extension_origin ||
      page_origin == extension_origin) {
    return true;
  }

  if (!ui::PageTransitionIsWebTriggerable(transition_type))
    return true;

  // Unreachable web page error page (to allow showing the icon of the
  // unreachable app on this page).
  if (frame_url == content::kUnreachableWebDataURL)
    return true;

  bool is_dev_tools = page_origin.SchemeIs(content::kChromeDevToolsScheme);
  // Note: we check |web_accessible_ids_| (rather than first looking up the
  // extension in the registry and checking that) to be more resistant against
  // timing attacks. This way, determining access for an extension that isn't
  // installed takes the same amount of time as determining access for an
  // extension with no web accessible resources. We aren't worried about any
  // extensions with web accessible resources, since those are inherently
  // identifiable.
  if (!is_dev_tools && !web_accessible_ids_.count(extension_origin.host()))
    return false;

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(resource_url);
  if (is_dev_tools) {
    // Allow the load in the case of a non-existent extension. We'll just get a
    // 404 from the browser process.
    // TODO(devlin): Can this happen? Does devtools potentially make requests
    // to non-existent extensions?
    if (!extension)
      return true;
    // Devtools (chrome-extension:// URLs are loaded into frames of devtools to
    // support the devtools extension APIs).
    if (!chrome_manifest_urls::GetDevToolsPage(extension).is_empty())
      return true;
  }

  DCHECK(extension);

  // Disallow loading of packaged resources for hosted apps. We don't allow
  // hybrid hosted/packaged apps. The one exception is access to icons, since
  // some extensions want to be able to do things like create their own
  // launchers.
  base::StringPiece resource_root_relative_path =
      resource_url.path_piece().empty() ? base::StringPiece()
                                        : resource_url.path_piece().substr(1);
  if (extension->is_hosted_app() &&
      !IconsInfo::GetIcons(extension)
          .ContainsPath(resource_root_relative_path)) {
    LOG(ERROR) << "Denying load of " << resource_url.spec() << " from "
               << "hosted app.";
    return false;
  }

  // Disallow loading of extension resources which are not explicitly listed
  // as web or WebView accessible if the manifest version is 2 or greater.
  if (!WebAccessibleResourcesInfo::IsResourceWebAccessible(
          extension, resource_url.path()) &&
      !WebviewInfo::IsResourceWebviewAccessible(
          extension, dispatcher_->webview_partition_id(),
          resource_url.path())) {
    std::string message = base::StringPrintf(
        "Denying load of %s. Resources must be listed in the "
        "web_accessible_resources manifest key in order to be loaded by "
        "pages outside the extension.",
        resource_url.spec().c_str());
    frame->AddMessageToConsole(
        blink::WebConsoleMessage(blink::mojom::ConsoleMessageLevel::kError,
                                 blink::WebString::FromUTF8(message)));
    return false;
  }

  return true;
}

}  // namespace extensions
