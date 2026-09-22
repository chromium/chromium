// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_site_settings_android.h"

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/extensions/extension_developer_private_bridge.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "net/base/url_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

void ShowExtensionSiteSettings(content::WebContents* web_contents,
                               const std::string& extension_id) {
  // A settings tab only resolves a URL to a page under SettingsInTabUrlNav;
  // without it the URL below would land on the main settings page, so the page
  // has to be named by its fragment instead, which is what the Java side does.
  if (!web_contents ||
      !base::FeatureList::IsEnabled(chrome::android::kSettingsInTabUrlNav)) {
    ExtensionDeveloperPrivateBridge::ShowSiteSettings(extension_id);
    return;
  }

  // e.g. chrome://settings/siteDetails?site=chrome-extension%3A%2F%2F<id>. The
  // page identifies a site by origin rather than by full URL.
  const GURL url = net::AppendQueryParameter(
      GURL(base::StrCat(
          {chrome::kChromeUISettingsURL, chrome::kAndroidSiteDetailsSubpage})),
      "site", Extension::CreateOriginFromExtensionId(extension_id).Serialize());

  web_contents->OpenURL(content::OpenURLParams::CreateBrowserInitiated(
                            url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
                            ui::PAGE_TRANSITION_LINK),
                        /*navigation_handle_callback=*/{});
}

}  // namespace extensions
