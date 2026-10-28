// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_url_utils.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include <string>

#include "base/strings/string_number_conversions.h"
#include "net/base/url_util.h"

namespace {

// Query parameter name of the profile customization URL.
constexpr char kStyleParamKey[] = "style";

}  // namespace

ProfileCustomizationStyle GetProfileCustomizationStyle(const GURL& url) {
  std::string style_str;
  int style_int;
  // Default style if the parameter is missing.
  ProfileCustomizationStyle style = ProfileCustomizationStyle::kDefault;
  if (net::GetValueForKeyInQuery(url, kStyleParamKey, &style_str) &&
      base::StringToInt(style_str, &style_int)) {
    style = static_cast<ProfileCustomizationStyle>(style_int);
  }
  return style;
}

GURL AppendProfileCustomizationQueryParams(const GURL& url,
                                           ProfileCustomizationStyle style) {
  GURL url_with_params = net::AppendQueryParameter(
      url, kStyleParamKey, base::NumberToString(static_cast<int>(style)));
  return url_with_params;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
