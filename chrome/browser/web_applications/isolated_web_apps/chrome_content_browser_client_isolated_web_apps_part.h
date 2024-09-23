// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHROME_CONTENT_BROWSER_CLIENT_ISOLATED_WEB_APPS_PART_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHROME_CONTENT_BROWSER_CLIENT_ISOLATED_WEB_APPS_PART_H_

#include "chrome/browser/chrome_content_browser_client_parts.h"

class ChromeContentBrowserClient;

namespace web_apps {

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

  static bool AreIsolatedWebAppsEnabled(
      content::BrowserContext* browser_context);

  void AppendExtraRendererCommandLineSwitches(
      base::CommandLine* command_line,
      content::RenderProcessHost& process) override;
};

}  // namespace web_apps

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CHROME_CONTENT_BROWSER_CLIENT_ISOLATED_WEB_APPS_PART_H_
