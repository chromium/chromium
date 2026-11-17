// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/chrome_web_contents_menu_helper.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/user_browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"

content::ContextMenuParams AddContextMenuParamsPropertiesFromPreferences(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  // Use the prefs of the user that the WebContents is shown for, which differs
  // from the profile of the WebContents in TopChromeWebUIProfile.
  Profile* profile = Profile::FromBrowserContext(
      content::GetUserBrowserContext(*web_contents));
  PrefService* prefs = profile->GetPrefs();

  if (!prefs->GetBoolean(
          prefs::kDefaultSearchProviderContextMenuAccessAllowed)) {
    return params;
  }

  content::ContextMenuParams enriched_params = params;
  // Setting the key implies the menu access is allowed.
  enriched_params
      .properties[prefs::kDefaultSearchProviderContextMenuAccessAllowed] = "";

  return enriched_params;
}
