// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/shortcuts/desktop_shortcuts_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace shortcuts {

bool CanCreateDesktopShortcut(content::WebContents* web_contents) {
  // Do not allow if the web_contents appear to be crashing.
  if (!web_contents || web_contents->IsCrashed()) {
    return false;
  }

  auto* const tab = tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab || !tab->GetBrowserWindowInterface()) {
    return false;
  }

  auto* browser_window_interface = tab->GetBrowserWindowInterface();
  Profile* profile = browser_window_interface->GetProfile();

  // Do not allow for Guest or OTR profiles.
  // System profiles have not been introduced here because they do not have a
  // browser.
  if (!profile || profile->IsGuestSession() || profile->IsOffTheRecord()) {
    return false;
  }

  // Do not allow for error pages (like network errors etc).
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry && entry->GetPageType() == content::PAGE_TYPE_ERROR) {
    return false;
  }

  // Do not allow if the site_url is invalid.
  const GURL site_url = web_contents->GetLastCommittedURL();
  if (!site_url.is_valid()) {
    return false;
  }

  // Only URLs that have a scheme of `HTTP/HTTPs` or `chrome-extension` is
  // allowed.
  bool is_valid_for_shortcuts = site_url.SchemeIsHTTPOrHTTPS();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  is_valid_for_shortcuts =
      is_valid_for_shortcuts || site_url.SchemeIs(extensions::kExtensionScheme);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return is_valid_for_shortcuts;
}

}  // namespace shortcuts
