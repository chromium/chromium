// Copyright 2014 The Chromium Authors. All rights reserved.
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
#include "components/suggestions/suggestions_pref_names.h"

namespace ntp_tiles {

namespace {

const char* kDictionaryKeyUrl = "url";
const char* kDictionaryKeyTitle = "title";

}  // namespace

CustomLinksStore::CustomLinksStore(PrefService* prefs) : prefs_(prefs) {
  DCHECK(prefs);
}

CustomLinksStore::~CustomLinksStore() = default;

std::vector<CustomLinksManager::Link> CustomLinksStore::RetrieveLinks() {
  std::vector<CustomLinksManager::Link> links;

  const base::ListValue* stored_links =
      prefs_->GetList(prefs::kCustomLinksList);

  for (const base::Value& link : stored_links->GetList()) {
    const base::Value* url_value = link.FindKey(kDictionaryKeyUrl);
    const base::Value* title_value = link.FindKey(kDictionaryKeyTitle);
    GURL url = GURL(url_value->GetString());
    if (!url_value || !title_value || !url.is_valid()) {
      ClearLinks();
      links.clear();
      return links;
    }
    links.emplace_back(CustomLinksManager::Link{
        std::move(url), base::UTF8ToUTF16(title_value->GetString())});
  }
  return links;
}

void CustomLinksStore::StoreLinks(
    const std::vector<CustomLinksManager::Link>& links) {
  base::Value::ListStorage new_link_list;
  for (const CustomLinksManager::Link& link : links) {
    base::DictionaryValue new_link;
    new_link.SetKey(kDictionaryKeyUrl, base::Value(link.url.spec()));
    new_link.SetKey(kDictionaryKeyTitle, base::Value(link.title));
    new_link_list.push_back(std::move(new_link));
  }
  prefs_->Set(prefs::kCustomLinksList, base::Value(std::move(new_link_list)));
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
