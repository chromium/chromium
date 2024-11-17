// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/utils.h"

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/gfx/text_elider.h"

namespace tab_groups {
namespace {
const char kChromeUINewTabURL[] = "chrome://newtab/";

// Tab title to be shown or synced when tab URL is in an unsupported scheme.
const char* kDefaultTitleOverride = "New Tab";

// Max number of chars for the title of a URL.
const int kMaxTitleChars = 64;
}  // namespace

bool AreLocalIdsPersisted() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return true;
#else
  return false;
#endif
}

std::string LocalTabGroupIDToString(const LocalTabGroupID& local_tab_group_id) {
  return local_tab_group_id.ToString();
}

std::optional<LocalTabGroupID> LocalTabGroupIDFromString(
    const std::string& serialized_local_tab_group_id) {
#if BUILDFLAG(IS_ANDROID)
  return base::Token::FromString(serialized_local_tab_group_id);
#else
  auto token = base::Token::FromString(serialized_local_tab_group_id);
  if (!token.has_value()) {
    return std::nullopt;
  }

  return tab_groups::TabGroupId::FromRawToken(token.value());
#endif
}

bool IsURLValidForSavedTabGroups(const GURL& gurl) {
  return gurl.SchemeIsHTTPOrHTTPS() || gurl == GURL(kChromeUINewTabURL);
}

std::pair<GURL, std::u16string> GetDefaultUrlAndTitle() {
  return std::make_pair(GURL(kChromeUINewTabURL),
                        base::ASCIIToUTF16(kDefaultTitleOverride));
}

std::u16string GetTitleFromUrlForDisplay(const GURL& url) {
  std::u16string title = url_formatter::FormatUrl(
      url,
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlOmitHTTPS,
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);

  std::u16string display_title;
  if (base::i18n::StringContainsStrongRTLChars(title)) {
    // Wrap the URL in an LTR embedding for proper handling of RTL characters.
    // (RFC 3987 Section 4.1 states that "Bidirectional IRIs MUST be rendered in
    // the same way as they would be if they were in a left-to-right
    // embedding".)
    base::i18n::WrapStringWithLTRFormatting(&title);
  }

  // The tab title for display is not a security surface, so we can do the easy
  // elision from the trailing end instead of more complicated elision, or
  // restricting to only showing the hostname.
  gfx::ElideString(title, kMaxTitleChars, &display_title);
  return display_title;
}

}  // namespace tab_groups
