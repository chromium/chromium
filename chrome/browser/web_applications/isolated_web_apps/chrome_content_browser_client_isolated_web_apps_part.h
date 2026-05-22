// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHROME_CONTENT_BROWSER_CLIENT_ISOLATED_WEB_APPS_PART_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHROME_CONTENT_BROWSER_CLIENT_ISOLATED_WEB_APPS_PART_H_

#include <optional>

#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "url/origin.h"

class ChromeContentBrowserClient;

namespace web_app {

// Implements the IWA portion of ChromeContentBrowserClient.
class ChromeContentBrowserClientIsolatedWebAppsPart
    : public ChromeContentBrowserClientParts {
 public:
  ChromeContentBrowserClientIsolatedWebAppsPart();
  ~ChromeContentBrowserClientIsolatedWebAppsPart() override;

  ChromeContentBrowserClientIsolatedWebAppsPart(
      const ChromeContentBrowserClientIsolatedWebAppsPart&) = delete;
  ChromeContentBrowserClientIsolatedWebAppsPart& operator=(
      const ChromeContentBrowserClientIsolatedWebAppsPart&) = delete;

 private:
  // For access to `AreIsolatedWebAppsEnabled`.
  friend class ::ChromeContentBrowserClient;

  static std::vector<blink::mojom::IsolatedAppPermissionPolicyEntryPtr>
  GetBaselinePermissionsPolicyForIsolatedWebApp(
      content::BrowserContext* browser_context,
      const url::Origin& iwa_origin);

  static void EnsureRequiredHeadersForIsolatedApp(
      content::BrowserContext* browser_context,
      const GURL& url,
      network::mojom::URLResponseHead* response_head,
      const std::optional<content::FrameTreeNodeId>& frame_tree_node);

  static bool AreIsolatedWebAppsEnabled(
      content::BrowserContext* browser_context);

  void AppendExtraRendererCommandLineSwitches(
      base::CommandLine* command_line,
      content::RenderProcessHost& process) override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHROME_CONTENT_BROWSER_CLIENT_ISOLATED_WEB_APPS_PART_H_
