// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/custom_links_store.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace ntp_tiles {

namespace {

const char* kDictionaryKeyUrl = "url";
const char* kDictionaryKeyTitle = "title";
const char* kDictionaryKeyIsMostVisited = "isMostVisited";

}  // namespace

CustomLinksStore::CustomLinksStore(PrefService* prefs) : prefs_(prefs) {
  DCHECK(prefs);
}

CustomLinksStore::~CustomLinksStore() = default;

std::vector<CustomLinksManager::Link> CustomLinksStore::RetrieveLinks() {
  std::vector<CustomLinksManager::Link> links;

  const base::Value::List& stored_links =
      prefs_->GetList(prefs::kCustomLinksList);

  for (const base::Value& link : stored_links) {
    const std::string* url_string =
        link.GetDict().FindString(kDictionaryKeyUrl);
    const std::string* title_string =
        link.GetDict().FindString(kDictionaryKeyTitle);
    const std::optional<bool> mv_value =
        link.GetDict().FindBool(kDictionaryKeyIsMostVisited);

    GURL url = GURL(url_string ? *url_string : std::string());
    if (!url_string || !title_string || !url.is_valid()) {
      ClearLinks();
      links.clear();
      return links;
    }
    // Assume false if this value was not stored.
    bool is_most_visited = mv_value.value_or(false);

    links.emplace_back(CustomLinksManager::Link{
        std::move(url), base::UTF8ToUTF16(*title_string), is_most_visited});
  }
  return links;
}

void CustomLinksStore::StoreLinks(
    const std::vector<CustomLinksManager::Link>& links) {
  base::Value::List new_link_list;
  for (const CustomLinksManager::Link& link : links) {
    base::Value::Dict new_link;
    new_link.Set(kDictionaryKeyUrl, link.url.spec());
    new_link.Set(kDictionaryKeyTitle, link.title);
    new_link.Set(kDictionaryKeyIsMostVisited, link.is_most_visited);
    new_link_list.Append(std::move(new_link));
  }
  prefs_->SetList(prefs::kCustomLinksList, std::move(new_link_list));
}

void CustomLinksStore::ClearLinks() {
  prefs_->ClearPref(prefs::kCustomLinksList);
}

// static
void CustomLinksStore::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterListPref(prefs::kCustomLinksList,
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

}  // namespace ntp_tiles
