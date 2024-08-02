// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"

#include <algorithm>
#include <sstream>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_concept.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::settings {

namespace {

std::vector<int> GetMessageIds(const SearchConcept* search_concept) {
  // Start with only the canonical ID.
  std::vector<int> alt_tag_message_ids{search_concept->canonical_message_id};

  // Add alternate IDs, if they exist.
  for (size_t i = 0; i < SearchConcept::kMaxAltTagsPerConcept; ++i) {
    int curr_alt_tag_message_id = search_concept->alt_tag_ids[i];
    if (curr_alt_tag_message_id == SearchConcept::kAltTagEnd) {
      break;
    }
    alt_tag_message_ids.push_back(curr_alt_tag_message_id);
  }

  return alt_tag_message_ids;
}

}  // namespace

SearchTagRegistry::ScopedTagUpdater::ScopedTagUpdater(
    SearchTagRegistry* registry)
    : registry_(registry) {}

SearchTagRegistry::ScopedTagUpdater::ScopedTagUpdater(ScopedTagUpdater&&) =
    default;

SearchTagRegistry::ScopedTagUpdater::~ScopedTagUpdater() {
  std::vector<const SearchConcept*> pending_adds;
  std::vector<const SearchConcept*> pending_removals;

  for (const auto& map_entry : pending_updates_) {
    const std::string& result_id = map_entry.first;
    const SearchConcept* search_concept = map_entry.second.first;
    bool is_pending_add = map_entry.second.second;

    // If tag metadata is present for this tag, it has already been added and is
    // present in LocalSearchService.
    bool is_concept_already_added =
        registry_->GetTagMetadata(result_id) != nullptr;

    // Only add concepts which are intended to be added and have not yet been
    // added; only remove concepts which are intended to be removed and have
    // already been added.
    if (is_pending_add && !is_concept_already_added) {
      pending_adds.push_back(search_concept);
    }
    if (!is_pending_add && is_concept_already_added) {
      pending_removals.push_back(search_concept);
    }
  }

  if (!pending_adds.empty()) {
    registry_->AddSearchTags(pending_adds);
  }
  if (!pending_removals.empty()) {
    registry_->RemoveSearchTags(pending_removals);
  }
}

void SearchTagRegistry::ScopedTagUpdater::AddSearchTags(
    const std::vector<SearchConcept>& search_tags) {
  ProcessPendingSearchTags(search_tags, /*is_pending_add=*/true);
}

void SearchTagRegistry::ScopedTagUpdater::RemoveSearchTags(
    const std::vector<SearchConcept>& search_tags) {
  ProcessPendingSearchTags(search_tags, /*is_pending_add=*/false);
}

void SearchTagRegistry::ScopedTagUpdater::ProcessPendingSearchTags(
    const std::vector<SearchConcept>& search_tags,
    bool is_pending_add) {
  for (const auto& search_concept : search_tags) {
    std::string result_id = ToResultId(search_concept);
    auto it = pending_updates_.find(result_id);
    if (it == pending_updates_.end()) {
      pending_updates_.emplace(
          std::piecewise_construct, std::forward_as_tuple(result_id),
          std::forward_as_tuple(&search_concept, is_pending_add));
    } else {
      it->second.second = is_pending_add;
    }
  }
}

SearchTagRegistry::SearchTagRegistry(
    local_search_service::LocalSearchServiceProxy* local_search_service_proxy) {
  local_search_service_proxy->GetIndex(
      local_search_service::IndexId::kCrosSettings,
      local_search_service::Backend::kLinearMap,
      index_remote_.BindNewPipeAndPassReceiver());
  DCHECK(index_remote_.is_bound());
}

SearchTagRegistry::~SearchTagRegistry() = default;

void SearchTagRegistry::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SearchTagRegistry::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

SearchTagRegistry::ScopedTagUpdater SearchTagRegistry::StartUpdate() {
  return ScopedTagUpdater(this);
}

void SearchTagRegistry::AddSearchTags(
    const std::vector<const SearchConcept*>& search_tags) {
  // Add each concept to the map. Note that it is safe to take the address of
  // each concept because all concepts are allocated via static
  // base::NoDestructor objects in the Get*SearchConcepts() helper functions.
  for (const auto* search_concept : search_tags) {
    result_id_to_metadata_list_map_[ToResultId(*search_concept)] =
        search_concept;
  }

  index_remote_->AddOrUpdate(
      ConceptVectorToDataVector(search_tags),
      base::BindOnce(&SearchTagRegistry::NotifyRegistryAdded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SearchTagRegistry::RemoveSearchTags(
    const std::vector<const SearchConcept*>& search_tags) {
  std::vector<std::string> data_ids;
  for (const auto* search_concept : search_tags) {
    std::string result_id = ToResultId(*search_concept);
    result_id_to_metadata_list_map_.erase(result_id);
    data_ids.push_back(std::move(result_id));
  }

  index_remote_->Delete(
      data_ids, base::BindOnce(&SearchTagRegistry::NotifyRegistryDeleted,
                               weak_ptr_factory_.GetWeakPtr()));
}

const SearchConcept* SearchTagRegistry::GetTagMetadata(
    const std::string& result_id) const {
  const auto it = result_id_to_metadata_list_map_.find(result_id);
  if (it == result_id_to_metadata_list_map_.end()) {
    return nullptr;
  }
  return it->second;
}

// static
std::string SearchTagRegistry::ToResultId(const SearchConcept& search_concept) {
  std::stringstream ss;
  switch (search_concept.type) {
    case mojom::SearchResultType::kSection:
      ss << search_concept.id.section;
      break;
    case mojom::SearchResultType::kSubpage:
      ss << search_concept.id.subpage;
      break;
    case mojom::SearchResultType::kSetting:
      ss << search_concept.id.setting;
      break;
  }
  ss << "," << search_concept.canonical_message_id;
  return ss.str();
}

std::vector<local_search_service::Data>
SearchTagRegistry::ConceptVectorToDataVector(
    const std::vector<const SearchConcept*>& search_tags) {
  std::vector<local_search_service::Data> data_list;

  for (const auto* search_concept : search_tags) {
    // Create a list of Content objects, which use the stringified version of
    // message IDs as identifiers.
    std::vector<local_search_service::Content> content_list;
    for (int message_id : GetMessageIds(search_concept)) {
      content_list.emplace_back(
          /*id=*/base::NumberToString(message_id),
          /*content=*/l10n_util::GetStringUTF16(message_id));
    }

    // Compute an identifier for this result; the same ID format it used in
    // GetTagMetadata().
    data_list.emplace_back(ToResultId(*search_concept),
                           std::move(content_list));
  }

  return data_list;
}

void SearchTagRegistry::NotifyRegistryUpdated() {
  for (auto& observer : observer_list_) {
    observer.OnRegistryUpdated();
  }
}

void SearchTagRegistry::NotifyRegistryAdded() {
  NotifyRegistryUpdated();
}

void SearchTagRegistry::NotifyRegistryDeleted(uint32_t num_deleted) {
  NotifyRegistryUpdated();
}

}  // namespace ash::settings
