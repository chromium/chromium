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

FileTypePoliciesOverrideResult ShouldOverrideFileTypePolicies(
    const std::string& extension,
    const GURL& url,
    const PrefService* prefs) {
  if (!url.is_valid()) {
    return FileTypePoliciesOverrideResult::kDoNotOverride;
  }

  // If the download is a local file, suppress "dangerous file" warnings because
  // they are not helpful at this point; the file is already on disk.
  if (url.SchemeIsFile() && url.host_piece().empty()) {
    return FileTypePoliciesOverrideResult::kOverrideAsNotDangerous;
  }

  // Check for a match on the list of exempt URL pattern and filetype pairs.
  // The list is supplied as a pref/policy.
  if (!prefs ||
      !prefs->HasPrefPath(
          file_type::prefs::
              kExemptDomainFileTypePairsFromFileTypeDownloadWarnings)) {
    return FileTypePoliciesOverrideResult::kDoNotOverride;
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
    url_matcher::util::AddFiltersWithLimit(&matcher, true, &id,
                                           domains_for_extension);
    GURL normalized_url =
        url.SchemeIsBlob() ? url::Origin::Create(url).GetURL() : url;
    if (!matcher.MatchURL(normalized_url).empty()) {
      return FileTypePoliciesOverrideResult::kOverrideAsNotDangerous;
    }
  }

  return FileTypePoliciesOverrideResult::kDoNotOverride;
}

}  // namespace safe_browsing
