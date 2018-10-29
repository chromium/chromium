// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace extensions {

namespace {

bool IsSiteSecure(const content::WebContents* web_contents) {
  const SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  if (helper) {
    security_state::SecurityInfo security_info;
    helper->GetSecurityInfo(&security_info);
    switch (security_info.security_level) {
      case security_state::SECURITY_LEVEL_COUNT:
        NOTREACHED();
        return false;
      case security_state::EV_SECURE:
      case security_state::SECURE:
      case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
        return true;
      case security_state::NONE:
      case security_state::HTTP_SHOW_WARNING:
      case security_state::DANGEROUS:
        return false;
    }
  }
  return false;
}

// Returns true if |app_url| and |page_url| are the same origin. To avoid
// breaking Hosted Apps and Bookmark Apps that might redirect to sites in the
// same domain but with "www.", this returns true if |page_url| is secure and in
// the same origin as |app_url| with "www.".
bool IsSameHostAndPort(const GURL& app_url, const GURL& page_url) {
  return (app_url.host_piece() == page_url.host_piece() ||
          std::string("www.") + app_url.host() == page_url.host_piece()) &&
         app_url.port() == page_url.port();
}

// Returns true if |page_url| is in the scope of the app for |app_url|. If the
// app has no scope defined (as in a bookmark app), we fall back to checking
// |page_url| has the same origin as |app_url|.
bool IsSameScope(const GURL& app_url,
                 const GURL& page_url,
                 content::BrowserContext* profile) {
  // We can only check scope on apps that are installed PWAs, so fall
  // back to same origin check if PWA windowing is not enabled
  // (GetInstalledPwaForUrl requires this).
  if (!base::FeatureList::IsEnabled(::features::kDesktopPWAWindowing))
    return IsSameHostAndPort(app_url, page_url);

  const Extension* app_for_window = extensions::util::GetInstalledPwaForUrl(
      profile, app_url, extensions::LAUNCH_CONTAINER_WINDOW);

  // We don't have a scope, fall back to same origin check.
  if (!app_for_window)
    return IsSameHostAndPort(app_url, page_url);

  return app_for_window ==
         extensions::util::GetInstalledPwaForUrl(
             profile, page_url, extensions::LAUNCH_CONTAINER_WINDOW);
}

// Gets the icon to use if the extension app icon is not available.
gfx::ImageSkia GetFallbackAppIcon(Browser* browser) {
  gfx::ImageSkia page_icon = browser->GetCurrentPageIcon().AsImageSkia();
  if (!page_icon.isNull())
    return page_icon;

  // The extension icon may be loading still. Return a transparent icon rather
  // than using a placeholder to avoid flickering.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

}  // namespace

const char kPwaWindowEngagementTypeHistogram[] =
    "Webapp.Engagement.EngagementType";

// static
bool HostedAppBrowserController::IsForExperimentalHostedAppBrowser(
    const Browser* browser) {
  return browser && browser->hosted_app_controller() &&
         base::FeatureList::IsEnabled(::features::kDesktopPWAWindowing);
}

// static
void HostedAppBrowserController::SetAppPrefsForWebContents(
    HostedAppBrowserController* controller,
    content::WebContents* web_contents) {
  auto* rvh = web_contents->GetRenderViewHost();

  web_contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  rvh->SyncRendererPrefs();

  // This function could be called for non Hosted Apps.
  if (!controller)
    return;

  extensions::TabHelper::FromWebContents(web_contents)
      ->SetExtensionApp(controller->GetExtension());

  web_contents->NotifyPreferencesChanged();
}

base::string16 HostedAppBrowserController::FormatUrlOrigin(const GURL& url) {
  return url_formatter::FormatUrl(
      url.GetOrigin(),
      url_formatter::kFormatUrlOmitUsernamePassword |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitHTTP |
          url_formatter::kFormatUrlOmitTrailingSlashOnBareHostname |
          url_formatter::kFormatUrlOmitTrivialSubdomains,
      net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
}

HostedAppBrowserController::HostedAppBrowserController(Browser* browser)
    : SiteEngagementObserver(SiteEngagementService::Get(browser->profile())),
      browser_(browser),
      extension_id_(web_app::GetAppIdFromApplicationName(browser->app_name())),
      // If a bookmark app has a URL handler, then it is a PWA.
      // TODO(https://crbug.com/774918): Replace once there is a more explicit
      // indicator of a Bookmark App for an installable website.
      created_for_installed_pwa_(
          base::FeatureList::IsEnabled(::features::kDesktopPWAWindowing) &&
          UrlHandlers::GetUrlHandlers(GetExtension())) {
  browser_->tab_strip_model()->AddObserver(this);
}

HostedAppBrowserController::~HostedAppBrowserController() {
  browser_->tab_strip_model()->RemoveObserver(this);
}

bool HostedAppBrowserController::ShouldShowLocationBar() const {
  // The extension can be null if this is invoked after uninstall.
  const Extension* extension = GetExtension();
  if (!extension)
    return false;

  DCHECK(extension->is_hosted_app());

  const content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();

  // Don't show a location bar until a navigation has occurred.
  if (!web_contents || web_contents->GetLastCommittedURL().is_empty())
    return false;

  GURL launch_url = AppLaunchInfo::GetLaunchWebURL(extension);
  base::StringPiece launch_scheme = launch_url.scheme_piece();

  bool is_internal_launch_scheme = launch_scheme == kExtensionScheme ||
                                   launch_scheme == content::kChromeUIScheme;

  GURL last_committed_url = web_contents->GetLastCommittedURL();

  // We check the visible URL to indicate to the user that they are navigating
  // to a different origin than that of the app as soon as the navigation
  // starts, even if the navigation hasn't committed yet.
  GURL visible_url = web_contents->GetVisibleURL();

  // The current page must be secure for us to hide the location bar. However,
  // the chrome-extension:// and chrome:// launch URL apps can hide the location
  // bar, if the current WebContents URLs are the same as the launch scheme.
  //
  // Note that the launch scheme may be insecure, but as long as the current
  // page's scheme is secure, we can hide the location bar.
  base::StringPiece secure_page_scheme =
      is_internal_launch_scheme ? launch_scheme : url::kHttpsScheme;

  // Insecure page schemes show the location bar.
  if (last_committed_url.scheme_piece() != secure_page_scheme ||
      visible_url.scheme_piece() != secure_page_scheme) {
    return true;
  }

  // Page URLs that are not within scope
  // (https://www.w3.org/TR/appmanifest/#dfn-within-scope) of the app
  // corresponding to |launch_url| show the location bar.
  if (!IsSameScope(launch_url, last_committed_url,
                   web_contents->GetBrowserContext()) ||
      !IsSameScope(launch_url, visible_url, web_contents->GetBrowserContext()))
    return true;

  // Insecure external web sites show the location bar.
  if (!is_internal_launch_scheme && !IsSiteSecure(web_contents))
    return true;

  return false;
}

void HostedAppBrowserController::UpdateLocationBarVisibility(
    bool animate) const {
  browser_->window()->GetLocationBar()->UpdateLocationBarVisibility(
      ShouldShowLocationBar(), animate);
}

gfx::ImageSkia HostedAppBrowserController::GetWindowAppIcon() const {
  // TODO(calamity): Use the app name to retrieve the app icon without using the
  // extensions tab helper to make icon load more immediate.
  content::WebContents* contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!contents)
    return GetFallbackAppIcon(browser_);

  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(contents);
  if (!extensions_tab_helper)
    return GetFallbackAppIcon(browser_);

  const SkBitmap* icon_bitmap = extensions_tab_helper->GetExtensionAppIcon();
  if (!icon_bitmap)
    return GetFallbackAppIcon(browser_);

  return gfx::ImageSkia::CreateFrom1xBitmap(*icon_bitmap);
}

gfx::ImageSkia HostedAppBrowserController::GetWindowIcon() const {
  if (IsForExperimentalHostedAppBrowser(browser_))
    return GetWindowAppIcon();

  return browser_->GetCurrentPageIcon().AsImageSkia();
}

base::Optional<SkColor> HostedAppBrowserController::GetThemeColor() const {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_->profile());
  const Extension* extension =
      registry->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
  if (extension) {
    const base::Optional<SkColor> color =
        AppThemeColorInfo::GetThemeColor(extension);
    if (color) {
      // The frame/tabstrip code expects an opaque color.
      return SkColorSetA(*color, SK_AlphaOPAQUE);
    }
  }
  return base::nullopt;
}

base::string16 HostedAppBrowserController::GetTitle() const {
  content::WebContents* web_contents =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return base::string16();

  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  return entry ? entry->GetTitle() : base::string16();
}

const Extension* HostedAppBrowserController::GetExtension() const {
  return ExtensionRegistry::Get(browser_->profile())
      ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
}

const Extension* HostedAppBrowserController::GetExtensionForTesting() const {
  return GetExtension();
}

std::string HostedAppBrowserController::GetAppShortName() const {
  return GetExtension()->short_name();
}

base::string16 HostedAppBrowserController::GetFormattedUrlOrigin() const {
  return FormatUrlOrigin(AppLaunchInfo::GetLaunchWebURL(GetExtension()));
}

bool HostedAppBrowserController::CanUninstall() const {
  return extensions::ExtensionSystem::Get(browser_->profile())
      ->management_policy()
      ->UserMayModifySettings(GetExtension(), nullptr);
}

void HostedAppBrowserController::Uninstall(UninstallReason reason,
                                           UninstallSource source) {
  uninstall_dialog_.reset(ExtensionUninstallDialog::Create(
      browser_->profile(), browser_->window()->GetNativeWindow(), this));
  uninstall_dialog_->ConfirmUninstall(GetExtension(), reason, source);
}

void HostedAppBrowserController::OnEngagementEvent(
    content::WebContents* web_contents,
    const GURL& /*url*/,
    double /*score*/,
    SiteEngagementService::EngagementType type) {
  if (!created_for_installed_pwa_)
    return;

  // Check the event belongs to the controller's associated browser window.
  if (!web_contents ||
      web_contents != browser_->tab_strip_model()->GetActiveWebContents()) {
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(kPwaWindowEngagementTypeHistogram, type,
                            SiteEngagementService::ENGAGEMENT_LAST);
}

void HostedAppBrowserController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& delta : change.deltas())
      OnTabInserted(delta.insert.contents);
  } else if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& delta : change.deltas())
      OnTabRemoved(delta.remove.contents);
  }
}

void HostedAppBrowserController::OnTabInserted(content::WebContents* contents) {
  HostedAppBrowserController::SetAppPrefsForWebContents(this, contents);
}

void HostedAppBrowserController::OnTabRemoved(content::WebContents* contents) {
  auto* rvh = contents->GetRenderViewHost();

  contents->GetMutableRendererPrefs()->can_accept_load_drops = true;
  rvh->SyncRendererPrefs();

  extensions::TabHelper::FromWebContents(contents)->SetExtensionApp(nullptr);

  contents->NotifyPreferencesChanged();
}

}  // namespace extensions
