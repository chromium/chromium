// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/keyword_cluster_finalizer.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/on_device_clustering_features.h"
#include "components/history_clusters/core/on_device_clustering_util.h"
#include "components/optimization_guide/core/entity_metadata.h"

namespace history_clusters {

namespace {

bool IsKeywordSimilarToVisitHost(
    const std::vector<std::u16string>& lowercase_host_parts,
    const std::u16string& keyword) {
  std::u16string lowercase_keyword = base::ToLowerASCII(keyword);
  if (base::Contains(lowercase_host_parts, keyword))
    return true;

  // Now check if the whitespace-stripped keyword is part of the host.
  std::u16string stripped_lowercase_keyword;
  base::RemoveChars(lowercase_keyword, base::kWhitespaceASCIIAs16,
                    &stripped_lowercase_keyword);
  return base::Contains(lowercase_host_parts, stripped_lowercase_keyword);
}

}  // namespace

KeywordClusterFinalizer::KeywordClusterFinalizer(
    const base::flat_map<std::string, optimization_guide::EntityMetadata>&
        entity_metadata_map)
    : entity_metadata_map_(entity_metadata_map) {}
KeywordClusterFinalizer::~KeywordClusterFinalizer() = default;

void KeywordClusterFinalizer::FinalizeCluster(history::Cluster& cluster) {
  base::flat_set<std::u16string> keywords_set;
  for (const auto& visit : cluster.visits) {
    if (!GetConfig().keyword_filter_on_noisy_visits && IsNoisyVisit(visit)) {
      // Do not put keywords if user visits the page a lot and it's not a
      // search-like visit.
      continue;
    }

    std::vector<std::u16string> lowercase_host_parts = base::SplitString(
        base::ToLowerASCII(
            base::UTF8ToUTF16(visit.annotated_visit.url_row.url().host())),
        u".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& entity :
         visit.annotated_visit.content_annotations.model_annotations.entities) {
      base::flat_set<std::u16string> entity_keywords;
      entity_keywords.insert(base::UTF8ToUTF16(entity.id));

      if (GetConfig().keyword_filter_on_entity_aliases) {
        auto it = entity_metadata_map_.find(entity.id);
        if (it != entity_metadata_map_.end()) {
          for (size_t i = 0; i < it->second.human_readable_aliases.size() &&
                             i < GetConfig().max_entity_aliases_in_keywords;
               i++) {
            entity_keywords.insert(
                base::UTF8ToUTF16(it->second.human_readable_aliases[i]));
          }
        }
      }

      if (!GetConfig().keyword_filter_on_visit_hosts) {
        // If we do not want any keywords associated with the visit host, make
        // sure that none of the keywords associated with the entity look like
        // they are for the visit host.
        for (const auto& entity_keyword : entity_keywords) {
          if (IsKeywordSimilarToVisitHost(lowercase_host_parts,
                                          entity_keyword)) {
            // One of the keywords is likely for the visit host, so clear out
            // the keywords for the whole entity.
            entity_keywords.clear();
            break;
          }
        }
      }

      if (!entity_keywords.empty()) {
        keywords_set.insert(entity_keywords.begin(), entity_keywords.end());
      }
    }
    if (GetConfig().keyword_filter_on_categories) {
      for (const auto& category : visit.annotated_visit.content_annotations
                                      .model_annotations.categories) {
        std::u16string category_u16string = base::UTF8ToUTF16(category.id);
        if (!GetConfig().keyword_filter_on_visit_hosts &&
            IsKeywordSimilarToVisitHost(lowercase_host_parts,
                                        category_u16string)) {
          continue;
        }
        keywords_set.insert(category_u16string);
      }
    }

    if (GetConfig().keyword_filter_on_search_terms &&
        !visit.annotated_visit.content_annotations.search_terms.empty()) {
      keywords_set.insert(
          visit.annotated_visit.content_annotations.search_terms);
    }
  }
  cluster.keywords =
      std::vector<std::u16string>(keywords_set.begin(), keywords_set.end());
}

}  // namespace history_clusters
