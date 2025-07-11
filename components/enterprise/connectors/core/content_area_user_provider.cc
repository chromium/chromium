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

bool IncludeContentAreaAccountEmail(const GURL& url) {
  if (!base::FeatureList::IsEnabled(kEnterpriseActiveUserDetection)) {
    return false;
  }

  static constexpr auto kWorkspaceDomains =
      base::MakeFixedFlatSet<std::string_view>({
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
      });

  for (const auto& domain : kWorkspaceDomains) {
    if (url.DomainIs(domain)) {
      return true;
    }
  }
  return false;
}

std::optional<size_t> GetUserIndex(const GURL& url) {
  const re2::RE2 kUserPathRegex{"/u/(\\d+)/"};

  int account_id = 0;
  if (re2::RE2::PartialMatch(url.path_piece(), kUserPathRegex, &account_id)) {
    return account_id;
  }

  std::string account_id_str;
  if (net::GetValueForKeyInQuery(url, "authuser", &account_id_str) &&
      base::StringToInt(account_id_str, &account_id)) {
    return account_id;
  }

  return std::nullopt;
}

}  // namespace

// static
std::string GetActiveContentAreaUser(signin::IdentityManager* im,
                                     const GURL& url) {
  if (!IncludeContentAreaAccountEmail(url)) {
    return "";
  }

  if (!im) {
    return "";
  }

  auto accounts = im->GetAccountsInCookieJar();

  if (accounts.GetAllAccounts().size() == 1) {
    return accounts.GetAllAccounts()[0].email;
  }

  size_t user_index = GetUserIndex(url).value_or(0);
  if (user_index >= accounts.GetAllAccounts().size()) {
    return "";
  }

  return accounts.GetAllAccounts()[user_index].email;
}

}  // namespace enterprise_connectors
