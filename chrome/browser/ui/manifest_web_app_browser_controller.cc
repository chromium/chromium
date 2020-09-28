// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/manifest_web_app_browser_controller.h"

#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

ManifestWebAppBrowserController::ManifestWebAppBrowserController(
    Browser* browser)
    : AppBrowserController(browser, /*app_id=*/base::nullopt) {}

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
  if (!blink::network_utils::IsOriginSecure(app_start_url_))
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

base::string16 ManifestWebAppBrowserController::GetAppShortName() const {
  return base::string16();
}

base::string16 ManifestWebAppBrowserController::GetFormattedUrlOrigin() const {
  return FormatUrlOrigin(GetAppStartUrl());
}

GURL ManifestWebAppBrowserController::GetAppStartUrl() const {
  return app_start_url_;
}

bool ManifestWebAppBrowserController::IsUrlInAppScope(const GURL& url) const {
  // Prefer to use manifest scope URL if available; fall back to app launch URL
  // if not available. Manifest fallback is always launch URL minus filename,
  // query, and fragment.
  const GURL scope_url = !manifest_scope_.is_empty()
                             ? manifest_scope_
                             : GetAppStartUrl().GetWithoutFilename();

  return IsInScope(url, scope_url);
}

void ManifestWebAppBrowserController::OnTabInserted(
    content::WebContents* contents) {
  // Since we are experimenting with multi-tab PWAs, we only try to load the
  // manifest if this is the first web contents being loaded in this window.
  DCHECK(!browser()->tab_strip_model()->empty());
  if (browser()->tab_strip_model()->count() == 1) {
    app_start_url_ = contents->GetURL();
    contents->GetManifest(
        base::BindOnce(&ManifestWebAppBrowserController::OnManifestLoaded,
                       weak_factory_.GetWeakPtr()));
  }
  AppBrowserController::OnTabInserted(contents);
  UpdateCustomTabBarVisibility(false);
}

void ManifestWebAppBrowserController::OnManifestLoaded(
    const GURL& manifest_url,
    const blink::Manifest& manifest) {
  manifest_scope_ = manifest.scope;
}

// static
bool ManifestWebAppBrowserController::IsInScope(const GURL& url,
                                                const GURL& scope) {
  if (!url::IsSameOriginWith(scope, url))
    return false;

  std::string scope_path = scope.path();
  if (base::EndsWith(scope_path, "/", base::CompareCase::SENSITIVE))
    scope_path = scope_path.substr(0, scope_path.length() - 1);

  const std::string url_path = url.path();
  return url_path == scope_path ||
         base::StartsWith(url_path, scope_path + "/",
                          base::CompareCase::SENSITIVE);
}
