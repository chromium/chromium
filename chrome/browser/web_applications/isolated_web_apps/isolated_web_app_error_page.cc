// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_error_page.h"

#include <string>

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool IsIwaAppInstalledInIwaDevModeWhichHasBeenDisabled(
    content::BrowserContext* browser_context,
    const GURL& url) {
  ASSIGN_OR_RETURN(auto url_info, web_app::IsolatedWebAppUrlInfo::Create(url),
                   [](auto) { return false; });
  Profile& profile = CHECK_DEREF(Profile::FromBrowserContext(browser_context));
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(&profile);
  if (!provider) {
    return false;
  }
  const web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
  ASSIGN_OR_RETURN(const web_app::WebApp& iwa,
                   web_app::GetIsolatedWebAppById(registrar, url_info.app_id()),
                   [](auto) { return false; });
  return iwa.isolation_data()->location().dev_mode() &&
         !web_app::IsIwaDevModeEnabled(&profile);
}

std::u16string GetNetErrorMessage(content::BrowserContext* browser_context,
                                  const GURL& url,
                                  net::Error error_code) {
  switch (error_code) {
    case net::ERR_HTTP_RESPONSE_CODE_FAILURE:
      return l10n_util::GetStringUTF16(
          IDS_ERRORPAGES_MESSAGE_IWA_PAGE_NOT_FOUND);
    case net::ERR_INVALID_WEB_BUNDLE:
      return l10n_util::GetStringUTF16(
          IDS_ERRORPAGES_MESSAGE_IWA_INVALID_WEB_BUNDLE);
    case net::ERR_CONNECTION_REFUSED:
      return l10n_util::GetStringUTF16(
          IDS_ERRORPAGES_MESSAGE_IWA_CONNECTION_REFUSED);
    default:
      break;
  }

  if (error_code == net::ERR_FAILED &&
      IsIwaAppInstalledInIwaDevModeWhichHasBeenDisabled(browser_context, url)) {
    return l10n_util::GetStringUTF16(
        IDS_ERRORPAGES_MESSAGE_IWA_INSTALLED_IN_DEV_MODE);
  }

  return base::UTF8ToUTF16(net::ErrorToString(error_code));
}

}  // namespace

namespace web_app {

content::mojom::AlternativeErrorPageOverrideInfoPtr
MaybeGetIsolatedWebAppErrorPageInfo(const GURL& url,
                                    content::RenderFrameHost* render_frame_host,
                                    content::BrowserContext* browser_context,
                                    int32_t error_code) {
  return ConstructWebAppErrorPage(
      url, render_frame_host, browser_context,
      GetNetErrorMessage(browser_context, url,
                         static_cast<net::Error>(error_code)),
      /*supplementary_icon=*/std::u16string());
}

}  // namespace web_app
