// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/incognito_allowed_url.h"

#include <string>
#include <string_view>

#include "build/build_config.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "url/gurl.h"

namespace {

bool IsHostAllowedInIncognito(const GURL& url) {
  std::string scheme = url.GetScheme();
  std::string_view host = url.host();
  if (scheme != content::kChromeUIScheme) {
    return true;
  }

  if (host == chrome::kChromeUIChromeSigninHost) {
#if BUILDFLAG(IS_WIN)
    // Allow incognito mode for the chrome-signin url if we only want to
    // retrieve the login scope token without touching any profiles. This
    // option is only available on Windows for use with Google Credential
    // Provider for Windows.
    return signin::GetSigninReasonForEmbeddedPromoURL(url) ==
           signin_metrics::Reason::kFetchLstOnly;
#else
    return false;
#endif  // BUILDFLAG(IS_WIN)
  }

  // Most URLs are allowed in incognito; the following are exceptions.
  // chrome://extensions is on the list because it redirects to
  // chrome://settings.
  return host != chrome::kChromeUIAppLauncherPageHost &&
         host != chrome::kChromeUISettingsHost &&
#if BUILDFLAG(IS_CHROMEOS)
         host != chrome::kChromeUIOSSettingsHost &&
#endif
         host != chrome::kChromeUIHelpHost &&
         host != chrome::kChromeUIHistoryHost &&
         host != chrome::kChromeUIExtensionsHost &&
         host != password_manager::kChromeUIPasswordManagerHost;
}

}  // namespace

bool IsURLAllowedInIncognito(const GURL& url) {
  if (url.GetScheme() == content::kViewSourceScheme) {
    // A view-source URL is allowed in incognito mode only if the URL itself
    // is allowed in incognito mode. Remove the "view-source:" from the start
    // of the URL and validate the rest.
    const size_t scheme_len = strlen(content::kViewSourceScheme);
    CHECK_GT(url.spec().size(), scheme_len);
    std::string_view stripped_url_str(url.spec());
    // Adding +1 for ':' character.
    stripped_url_str.remove_prefix(scheme_len + 1);
    const GURL stripped_url(stripped_url_str);
    if (stripped_url.is_empty()) {
      return true;
    }
    return stripped_url.is_valid() && IsURLAllowedInIncognito(stripped_url);
  }

  return IsHostAllowedInIncognito(url);
}
