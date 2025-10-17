// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/content_area_user_provider.h"

#include "base/containers/fixed_flat_set.h"
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

const std::set<std::string_view>& GoogleDomains() {
  static const std::set<std::string_view> kDomains = {"google.com"};
  return kDomains;
}

const std::set<std::string_view>& TabWorkspaceDomains() {
  static const std::set<std::string_view> kDomains = {
      "mail.google.com",        "meet.google.com",
      "calendar.google.com",    "drive.google.com",
      "docs.google.com",        "sites.google.com",
      "keep.google.com",        "script.google.com",
      "cloudsearch.google.com", "console.cloud.google.com",
      "datastudio.google.com",  "gemini.google.com",
  };
  return kDomains;
}

const std::set<std::string_view>& FrameWorkspaceDomains() {
  static const std::set<std::string_view> kDomains = {
      "ogs.google.com",
  };
  return kDomains;
}

bool IncludeContentAreaAccountEmail(
    const GURL& url,
    const std::set<std::string_view>& allowed_domains) {
  if (!base::FeatureList::IsEnabled(kEnterpriseActiveUserDetection)) {
    return false;
  }

  for (const auto& domain : allowed_domains) {
    if (url.DomainIs(domain)) {
      return true;
    }
  }
  return false;
}

std::optional<size_t> GetUserIndex(const GURL& url) {
  const re2::RE2 kUserPathRegex{"/u/(\\d+)/"};

  int account_id = 0;
  if (re2::RE2::PartialMatch(url.path(), kUserPathRegex, &account_id)) {
    return account_id;
  }

  std::string account_id_str;
  if (net::GetValueForKeyInQuery(url, "authuser", &account_id_str) &&
      base::StringToInt(account_id_str, &account_id)) {
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
  if (!IncludeContentAreaAccountEmail(tab_url, GoogleDomains())) {
    return "";
  }

  return GetEmailFromUrl(im, tab_url);
}

std::string GetActiveFrameUser(signin::IdentityManager* im,
                               const GURL& tab_url,
                               const GURL& frame_url) {
  if (!IncludeContentAreaAccountEmail(tab_url, TabWorkspaceDomains()) ||
      !IncludeContentAreaAccountEmail(frame_url, FrameWorkspaceDomains())) {
    return "";
  }

  return GetEmailFromUrl(im, frame_url);
}

bool CanRetrieveActiveUser(const GURL& tab_url) {
  return IncludeContentAreaAccountEmail(tab_url, GoogleDomains());
}

}  // namespace enterprise_connectors
