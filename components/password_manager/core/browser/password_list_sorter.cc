// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_list_sorter.h"

#include <algorithm>
#include <tuple>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

constexpr char kSortKeyPartsSeparator = ' ';

// The character that is added to a sort key if there is no federation.
// Note: to separate the entries w/ federation and the entries w/o federation,
// this character should be alphabetically smaller than real federations.
constexpr char kSortKeyNoFederationSymbol = '-';

}  // namespace

std::string CreateSortKey(const autofill::PasswordForm& form) {
  std::string shown_origin;
  GURL link_url;
  std::tie(shown_origin, link_url) = GetShownOriginAndLinkUrl(form);

  const auto facet_uri =
      FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
  const bool is_android_uri = facet_uri.IsValidAndroidFacetURI();

  if (is_android_uri) {
    // In case of Android credentials |GetShownOriginAndLinkURl| might return
    // the app display name, e.g. the Play Store name of the given application.
    // This might or might not correspond to the eTLD+1, which is why
    // |shown_origin| is set to the reversed android package name in this case,
    // e.g. com.example.android => android.example.com.
    shown_origin = SplitByDotAndReverse(facet_uri.android_package_name());
  }

  std::string site_name =
      net::registry_controlled_domains::GetDomainAndRegistry(
          shown_origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (site_name.empty())  // e.g. localhost.
    site_name = shown_origin;

  std::string key = site_name + kSortKeyPartsSeparator;

  // Since multiple distinct credentials might have the same site name, more
  // information is added. For Android credentials this includes the full
  // canonical spec which is guaranteed to be unique for a given App.
  key += is_android_uri ? facet_uri.canonical_spec()
                        : SplitByDotAndReverse(shown_origin);

  if (!form.blacklisted_by_user) {
    key += kSortKeyPartsSeparator + base::UTF16ToUTF8(form.username_value) +
           kSortKeyPartsSeparator + base::UTF16ToUTF8(form.password_value);

    key += kSortKeyPartsSeparator;
    if (!form.federation_origin.opaque())
      key += form.federation_origin.host();
    else
      key += kSortKeyNoFederationSymbol;
  }

  // To separate HTTP/HTTPS credentials, add the scheme to the key.
  return key += kSortKeyPartsSeparator + link_url.scheme();
}

void SortEntriesAndHideDuplicates(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* list,
    DuplicatesMap* duplicates) {
  std::vector<std::pair<std::string, std::unique_ptr<autofill::PasswordForm>>>
      keys_to_forms;
  keys_to_forms.reserve(list->size());
  for (auto& form : *list) {
    std::string key = CreateSortKey(*form);
    keys_to_forms.emplace_back(std::move(key), std::move(form));
  }

  std::sort(keys_to_forms.begin(), keys_to_forms.end());

  list->clear();
  duplicates->clear();
  std::string previous_key;
  for (auto& key_to_form : keys_to_forms) {
    if (key_to_form.first != previous_key) {
      list->push_back(std::move(key_to_form.second));
      previous_key = key_to_form.first;
    } else {
      duplicates->emplace(previous_key, std::move(key_to_form.second));
    }
  }
}

}  // namespace password_manager
