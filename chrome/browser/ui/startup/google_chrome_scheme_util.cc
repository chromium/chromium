// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/google_chrome_scheme_util.h"

#include <string>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/webui_url_constants.h"
#else
#include "chrome/common/chrome_constants.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/headless/headless_mode_util.h"
#endif

namespace startup {

bool StripGoogleChromeScheme(base::FilePath::StringViewType& arg) {
  const std::string scheme = shell_integration::GetDirectLaunchUrlScheme();
  if (scheme.empty()) {
    return false;  // Direct launch not supported.
  }
  const base::FilePath kFullPrefixPath = base::FilePath::FromASCII(
      base::StrCat({scheme, url::kStandardSchemeSeparator}));
  // Note: we enabled the feature flag condition later
  // we want to activate the experiment when it is relevant for better
  // stats collection. We plan to remove this flag once we establish it works
  // fine.
  if (auto suffix = base::RemovePrefix(arg, kFullPrefixPath.value(),
                                       base::CompareCase::INSENSITIVE_ASCII);
      suffix && base::FeatureList::IsEnabled(features::kGoogleChromeScheme)) {
    arg = *suffix;
    return true;
  }
  return false;
}

bool ValidateUrl(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }

  const GURL settings_url(chrome::kChromeUISettingsURL);
  bool url_points_to_an_approved_settings_page = false;
#if BUILDFLAG(IS_CHROMEOS)
  // In ChromeOS, allow any settings page to be specified on the command line.
  url_points_to_an_approved_settings_page =
      url.DeprecatedGetOriginAsURL() == settings_url.DeprecatedGetOriginAsURL();
#else
  // Exposed for external cleaners to offer a settings reset to the
  // user. The allowed URLs must match exactly.
  const GURL reset_settings_url =
      settings_url.Resolve(chrome::kResetProfileSettingsSubPage);
  url_points_to_an_approved_settings_page = url == reset_settings_url;
#endif  // BUILDFLAG(IS_CHROMEOS)

  bool url_scheme_is_chrome = false;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // In Headless mode, allow any URL pattern that matches chrome:// scheme if
  // the user explicitly allowed it.
  if (headless::IsHeadlessMode() && url.SchemeIs(content::kChromeUIScheme)) {
    if (headless::IsChromeSchemeUrlAllowed()) {
      url_scheme_is_chrome = true;
    } else {
      LOG(WARNING) << "Headless mode requires the --allow-chrome-scheme-url "
                      "command-line option to access "
                   << url;
    }
  }
#endif

  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  return policy->IsWebSafeScheme(url.GetScheme()) ||
         url.SchemeIs(url::kFileScheme) || url_scheme_is_chrome ||
         url_points_to_an_approved_settings_page ||
         url.spec() == url::kAboutBlankURL;
}

}  // namespace startup
