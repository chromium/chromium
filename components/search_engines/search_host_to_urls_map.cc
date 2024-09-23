// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_host_to_urls_map.h"

#include <memory>
#include <string_view>

#include "base/ranges/algorithm.h"
#include "components/search_engines/template_url.h"

SearchHostToURLsMap::SearchHostToURLsMap()
    : initialized_(false) {
}

SearchHostToURLsMap::~SearchHostToURLsMap() {
}

void SearchHostToURLsMap::Init(
    const TemplateURL::OwnedTemplateURLVector& template_urls,
    const SearchTermsData& search_terms_data) {
  DCHECK(!initialized_);
  initialized_ = true;  // Set here so Add doesn't assert.
  Add(template_urls, search_terms_data);
}

void SearchHostToURLsMap::Add(TemplateURL* template_url,
                              const SearchTermsData& search_terms_data) {
  DCHECK(initialized_);
  DCHECK(template_url);
  DCHECK_NE(TemplateURL::OMNIBOX_API_EXTENSION, template_url->type());

  const GURL url(template_url->GenerateSearchURL(search_terms_data));
  if (!url.is_valid() || !url.has_host())
    return;

  host_to_urls_map_[url.host()].insert(template_url);
}

void SearchHostToURLsMap::Remove(const TemplateURL* template_url) {
  DCHECK(initialized_);
  DCHECK(template_url);
  DCHECK_NE(TemplateURL::OMNIBOX_API_EXTENSION, template_url->type());

  // A given TemplateURL only occurs once in the map.
  auto set_with_url = base::ranges::find_if(
      host_to_urls_map_,
      [&](std::pair<const std::string, TemplateURLSet>& entry) {
        return entry.second.erase(template_url);
      });

  if (set_with_url != host_to_urls_map_.end() && set_with_url->second.empty())
    host_to_urls_map_.erase(set_with_url);
}

TemplateURL* SearchHostToURLsMap::GetTemplateURLForHost(std::string_view host) {
  DCHECK(initialized_);

  HostToURLsMap::const_iterator iter = host_to_urls_map_.find(host);
  if (iter == host_to_urls_map_.end() || iter->second.empty())
    return nullptr;

  // Because we have to happily tolerate duplicates in TemplateURLService now,
  /// return the best TemplateURL for `host`, just like
  // `GetTemplateURLForKeyword` returns the best TemplateURL for a keyword.
  return *std::min_element(iter->second.begin(), iter->second.end(),
                           [](const auto& a, const auto& b) {
                             return a->IsBetterThanConflictingEngine(b);
                           });
}

SearchHostToURLsMap::TemplateURLSet* SearchHostToURLsMap::GetURLsForHost(
    std::string_view host) {
  DCHECK(initialized_);

  auto urls_for_host = host_to_urls_map_.find(host);
  if (urls_for_host == host_to_urls_map_.end() || urls_for_host->second.empty())
    return nullptr;
  return &urls_for_host->second;
}

void SearchHostToURLsMap::Add(
    const TemplateURL::OwnedTemplateURLVector& template_urls,
    const SearchTermsData& search_terms_data) {
  for (const auto& turl : template_urls)
    Add(turl.get(), search_terms_data);
}
