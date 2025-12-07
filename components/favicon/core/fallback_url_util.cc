// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/fallback_url_util.h"

#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace {
constexpr char16_t kFallbackIconTextForIP[] = u"IP";
#if BUILDFLAG(IS_IOS)
constexpr char16_t kFallbackIconTextForAndroidApp[] = u"A";
#endif
}  // namespace

namespace favicon {

std::u16string GetFallbackIconText(const GURL& url) {
  if (url.is_empty()) {
    return std::u16string();
  }

  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain.empty()) {  // E.g., http://localhost/ or http://192.168.0.1/
    if (url.HostIsIPAddress()) {
      return kFallbackIconTextForIP;
    }
    domain = url.GetHost();

#if BUILDFLAG(IS_IOS)
    // Return "A" if it's an Android app URL. iOS only.
    if (url.is_valid() && url.spec().rfind("android://", 0) == 0) {
      return kFallbackIconTextForAndroidApp;
    }
#endif
  }

  if (domain.empty()) {
    return std::u16string();
  }

  std::u16string utf16_string = url_formatter::IDNToUnicode(domain);
  base::i18n::BreakIterator iter(utf16_string,
                                 base::i18n::BreakIterator::BREAK_CHARACTER);
  if (!iter.Init()) {
    return std::u16string();
  }

  if (!iter.Advance()) {
    return std::u16string();
  }

  return base::i18n::ToUpper(iter.GetString());
}

}  // namespace favicon
