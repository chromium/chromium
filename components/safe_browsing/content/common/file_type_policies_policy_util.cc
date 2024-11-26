// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/common/file_type_policies_policy_util.h"

#include "base/strings/string_util.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/common/file_type_policies_prefs.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace safe_browsing {

namespace {

constexpr char kFileExtensionNameKey[] = "file_extension";
constexpr char kDomainListKey[] = "domains";

}  // namespace

bool IsInNotDangerousOverrideList(const std::string& extension,
                                  const GURL& url,
                                  const PrefService* prefs) {
  GURL normalized_url = url;
  if (normalized_url.SchemeIsBlob()) {
    normalized_url = url::Origin::Create(normalized_url).GetURL();
  }

  // no overrides if we don't have this policy set or the url is invalid.
  if (!prefs || !normalized_url.is_valid() ||
      !prefs->HasPrefPath(
          file_type::prefs::
              kExemptDomainFileTypePairsFromFileTypeDownloadWarnings)) {
    return false;
  }
  const base::Value::List& heuristic_overrides = prefs->GetList(
      file_type::prefs::kExemptDomainFileTypePairsFromFileTypeDownloadWarnings);

  const std::string lower_extension = base::ToLowerASCII(extension);

  base::Value::List domains_for_extension;
  for (const base::Value& entry : heuristic_overrides) {
    const base::Value::Dict& extension_domain_patterns_dict = entry.GetDict();
    const std::string* extension_for_this_entry =
        extension_domain_patterns_dict.FindString(kFileExtensionNameKey);
    if (extension_for_this_entry &&
        base::ToLowerASCII(*extension_for_this_entry) == lower_extension) {
      const base::Value::List* domains_for_this_entry =
          extension_domain_patterns_dict.FindList(kDomainListKey);
      if (domains_for_this_entry) {
        for (const base::Value& domain : *domains_for_this_entry) {
          domains_for_extension.Append(domain.Clone());
        }
      }
    }
  }

  if (!domains_for_extension.empty()) {
    url_matcher::URLMatcher matcher;
    base::MatcherStringPattern::ID id(0);
    url_matcher::util::AddFilters(&matcher, true, &id, domains_for_extension);
    auto matching_set_size = matcher.MatchURL(normalized_url).size();
    return matching_set_size > 0;
  }

  return false;
}

}  // namespace safe_browsing
