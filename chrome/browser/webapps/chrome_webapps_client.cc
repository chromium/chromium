// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/chrome_webapps_client.h"

#include "base/logging.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "components/webapps/isolated_web_apps/scheme.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

namespace webapps {

bool ChromeWebappsClient::IsOriginConsideredSecure(const url::Origin& origin) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  return origin.scheme() == webapps::kIsolatedAppScheme;
#else   // !(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS))
  return false;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
}

security_state::SecurityLevel
ChromeWebappsClient::GetSecurityLevelForWebContents(
    content::WebContents* web_contents) {
  return SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityLevel();
}

infobars::ContentInfoBarManager*
ChromeWebappsClient::GetInfoBarManagerForWebContents(
    content::WebContents* web_contents) {
  return infobars::ContentInfoBarManager::FromWebContents(web_contents);
}

std::optional<webapps::AppId> ChromeWebappsClient::GetAppIdForWebContents(
    content::WebContents* web_contents) {
  return std::nullopt;
}

}  // namespace webapps
