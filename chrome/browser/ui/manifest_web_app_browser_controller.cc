// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/manifest_web_app_browser_controller.h"

#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

ManifestWebAppBrowserController::ManifestWebAppBrowserController(
    Browser* browser)
    : AppBrowserController(browser, /*app_id=*/base::nullopt),
      app_launch_url_(GURL()) {}

ManifestWebAppBrowserController::~ManifestWebAppBrowserController() = default;

bool ManifestWebAppBrowserController::HasMinimalUiButtons() const {
  return false;
}

bool ManifestWebAppBrowserController::ShouldShowCustomTabBar() const {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Don't show until a navigation has occurred.
  if (!web_contents || web_contents->GetLastCommittedURL().is_empty())
    return false;

  // Show if the web_contents is not on a secure origin.
  if (!content::IsOriginSecure(app_launch_url_))
    return true;

  // Show if web_contents is not currently in scope.
  if (!IsUrlInAppScope(web_contents->GetLastCommittedURL()) ||
      !IsUrlInAppScope(web_contents->GetVisibleURL())) {
    return true;
  }

  // Show if on a insecure external website. This checks the security level,
  // different from IsOriginSecure which just checks the origin itself.
  if (!InstallableManager::IsContentSecure(web_contents))
    return true;

  return false;
}

gfx::ImageSkia ManifestWebAppBrowserController::GetWindowAppIcon() const {
  gfx::ImageSkia page_icon = browser()->GetCurrentPageIcon().AsImageSkia();
  if (!page_icon.isNull())
    return page_icon;

  // The extension icon may be loading still. Return a transparent icon rather
  // than using a placeholder to avoid flickering.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

gfx::ImageSkia ManifestWebAppBrowserController::GetWindowIcon() const {
  return browser()->GetCurrentPageIcon().AsImageSkia();
}

std::string ManifestWebAppBrowserController::GetAppShortName() const {
  return std::string();
}

base::string16 ManifestWebAppBrowserController::GetFormattedUrlOrigin() const {
  return FormatUrlOrigin(GetAppLaunchURL());
}

GURL ManifestWebAppBrowserController::GetAppLaunchURL() const {
  return app_launch_url_;
}

bool ManifestWebAppBrowserController::IsUrlInAppScope(const GURL& url) const {
  // TODO(981703): Use the scope in the manifest instead of same origin check.
  return url::IsSameOriginWith(GetAppLaunchURL(), url);
}

void ManifestWebAppBrowserController::OnTabInserted(
    content::WebContents* contents) {
  if (app_launch_url_.is_empty())
    app_launch_url_ = contents->GetURL();
  AppBrowserController::OnTabInserted(contents);
  UpdateCustomTabBarVisibility(false);
}
