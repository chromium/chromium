// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/fallback_url_util.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace {
const char* kFallbackIconTextForIP = "IP";
#if BUILDFLAG(IS_IOS)
const char* kFallbackIconTextForAndroidApp = "A";
#endif
}  // namespace

namespace favicon {

std::u16string GetFallbackIconText(const GURL& url) {
  if (url.is_empty())
    return std::u16string();
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain.empty()) {  // E.g., http://localhost/ or http://192.168.0.1/
    if (url.HostIsIPAddress())
      return base::ASCIIToUTF16(kFallbackIconTextForIP);
    domain = url.host();

#if BUILDFLAG(IS_IOS)
    // Return "A" if it's an Android app URL. iOS only.
    if (url.is_valid() && url.spec().rfind("android://", 0) == 0)
      return base::ASCIIToUTF16(kFallbackIconTextForAndroidApp);
#endif
  }
  if (domain.empty())
    return std::u16string();
  // TODO(huangs): Handle non-ASCII ("xn--") domain names.
  return base::ASCIIToUTF16(base::ToUpperASCII(domain.substr(0, 1)));
}

}  // namespace favicon
