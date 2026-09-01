// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/content_area_user_provider.h"

#include <array>
#include <string_view>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"


namespace enterprise_connectors {

namespace {

constexpr auto kGoogleDomains = std::to_array<std::string_view>({
    "google.com",
});

constexpr auto kTabWorkspaceDomains = std::to_array<std::string_view>({
    "mail.google.com",
    "meet.google.com",
    "calendar.google.com",
    "drive.google.com",
    "docs.google.com",
    "sites.google.com",
    "keep.google.com",
    "script.google.com",
    "cloudsearch.google.com",
    "console.cloud.google.com",
    "datastudio.google.com",
    "gemini.google.com",
});

constexpr auto kFrameWorkspaceDomains = std::to_array<std::string_view>({
    "ogs.google.com",
});

bool IncludeContentAreaAccountEmail(
    const GURL& url,
    base::span<const std::string_view> allowed_domains) {
  for (std::string_view domain : allowed_domains) {
    if (url.DomainIs(domain)) {
      return true;
    }
  }
  return false;
}

std::optional<size_t> GetUserIndex(const GURL& url) {
  int account_id = 0;
  std::string account_id_str;
  if (net::GetValueForKeyInQuery(url, "authuser", &account_id_str) &&
      base::StringToInt(account_id_str, &account_id)) {
    return account_id;
  }

  const re2::RE2 kUserPathRegex{"/u/(\\d+)/"};
  if (re2::RE2::PartialMatch(url.path(), kUserPathRegex, &account_id)) {
    return account_id;
  }

  return std::nullopt;
}

std::string GetEmailFromUrl(signin::IdentityManager* im, const GURL& url) {
  if (!im) {
    return "";
  }

  auto accounts = im->GetAccountsInCookieJar();

  if (accounts.GetAllAccounts().size() == 1) {
    return accounts.GetAllAccounts()[0].email;
  }

  std::optional<size_t> user_index = GetUserIndex(url);
  if (!user_index.has_value()) {
    return "";
  }

  if (*user_index >= accounts.GetAllAccounts().size()) {
    return "";
  }

  return accounts.GetAllAccounts()[*user_index].email;
}

}  // namespace

std::string GetActiveContentAreaUser(signin::IdentityManager* im,
                                     const GURL& tab_url) {
  if (!IncludeContentAreaAccountEmail(tab_url, kGoogleDomains)) {
    return "";
  }

  return GetEmailFromUrl(im, tab_url);
}

std::string GetActiveFrameUser(signin::IdentityManager* im,
                               const GURL& tab_url,
                               const GURL& frame_url) {
  if (!IncludeContentAreaAccountEmail(tab_url, kTabWorkspaceDomains) ||
      !IncludeContentAreaAccountEmail(frame_url, kFrameWorkspaceDomains)) {
    return "";
  }

  return GetEmailFromUrl(im, frame_url);
}

std::string GetDefaultActiveUser(signin::IdentityManager* im, const GURL& url) {
  if (!im || !IncludeContentAreaAccountEmail(url, kGoogleDomains)) {
    return "";
  }

  auto accounts = im->GetAccountsInCookieJar();
  if (accounts.GetAllAccounts().size() >= 1) {
    return accounts.GetAllAccounts()[0].email;
  }
  return "";
}

std::string GetNavigationActiveContentAreaUser(signin::IdentityManager* im,
                                               const GURL& tab_url) {
  std::string email = GetActiveContentAreaUser(im, tab_url);
  if (!email.empty()) {
    return email;
  }

  return GetDefaultActiveUser(im, tab_url);
}

bool CanRetrieveActiveUser(const GURL& tab_url) {
  return IncludeContentAreaAccountEmail(tab_url, kGoogleDomains);
}

}  // namespace enterprise_connectors
