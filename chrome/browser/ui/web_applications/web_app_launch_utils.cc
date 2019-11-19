// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "content/public/browser/web_contents.h"

namespace {

Browser* ReparentWebContentsWithBrowserCreateParams(
    content::WebContents* contents,
    const Browser::CreateParams& browser_params) {
  Browser* source_browser = chrome::FindBrowserWithWebContents(contents);
  Browser* target_browser = Browser::Create(browser_params);

  TabStripModel* source_tabstrip = source_browser->tab_strip_model();
  // Avoid causing the existing browser window to close if this is the last tab
  // remaining.
  if (source_tabstrip->count() == 1)
    chrome::NewTab(source_browser);
  target_browser->tab_strip_model()->AppendWebContents(
      source_tabstrip->DetachWebContentsAt(
          source_tabstrip->GetIndexOfWebContents(contents)),
      true);
  target_browser->window()->Show();

  return target_browser;
}

}  // namespace

namespace web_app {

base::Optional<AppId> GetPwaForSecureActiveTab(Browser* browser) {
  switch (browser->location_bar_model()->GetSecurityLevel()) {
    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED();
      FALLTHROUGH;
    case security_state::NONE:
    case security_state::WARNING:
    case security_state::DANGEROUS:
      return base::nullopt;
    case security_state::EV_SECURE:
    case security_state::SECURE:
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      break;
  }
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  WebAppProvider* provider = WebAppProvider::Get(browser->profile());
  if (!provider)
    return base::nullopt;

  return provider->registrar().FindAppWithUrlInScope(
      web_contents->GetMainFrame()->GetLastCommittedURL());
}

Browser* ReparentWebAppForSecureActiveTab(Browser* browser) {
  base::Optional<AppId> app_id = GetPwaForSecureActiveTab(browser);
  if (!app_id)
    return nullptr;
  return ReparentWebContentsIntoAppBrowser(
      browser->tab_strip_model()->GetActiveWebContents(), *app_id);
}

Browser* ReparentWebContentsIntoAppBrowser(content::WebContents* contents,
                                           const AppId& app_id) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  // Incognito tabs reparent correctly, but remain incognito without any
  // indication to the user, so disallow it.
  DCHECK(!profile->IsOffTheRecord());
  Browser::CreateParams browser_params(Browser::CreateParams::CreateForApp(
      GenerateApplicationNameFromAppId(app_id), true /* trusted_source */,
      gfx::Rect(), profile, true /* user_gesture */));
  return ReparentWebContentsWithBrowserCreateParams(contents, browser_params);
}

Browser* ReparentWebContentsForFocusMode(content::WebContents* contents) {
  DCHECK(base::FeatureList::IsEnabled(features::kFocusMode));
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  // TODO(crbug.com/941577): Remove DCHECK when focus mode is permitted in guest
  // and incognito sessions.
  DCHECK(!profile->IsOffTheRecord());
  Browser::CreateParams browser_params(Browser::CreateParams::CreateForApp(
      GenerateApplicationNameForFocusMode(), true /* trusted_source */,
      gfx::Rect(), profile, true /* user_gesture */));
  browser_params.is_focus_mode = true;
  return ReparentWebContentsWithBrowserCreateParams(contents, browser_params);
}

}  // namespace web_app
