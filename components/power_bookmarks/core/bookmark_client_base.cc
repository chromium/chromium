// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/core/bookmark_client_base.h"

#include <string>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"

namespace power_bookmarks {

const char kSaveLocationStateHistogramBase[] =
    "PowerBookmarks.SuggestedSaveLocation.";

const base::TimeDelta kRejectionCoolOffTime = base::Minutes(1);

namespace {

const char kWasSuggestedFolderKey[] = "was_folder_suggested";

const char kTrue[] = "true";
const char kFalse[] = "false";
}  // namespace

BookmarkClientBase::BookmarkClientBase() = default;
BookmarkClientBase::~BookmarkClientBase() = default;

void BookmarkClientBase::Init(bookmarks::BookmarkModel* model) {
  bookmark_model_ = model;
  node_move_observer_ = std::make_unique<NodeMoveObserver>(this);
  model_observation_ = std::make_unique<
      base::ScopedObservation<bookmarks::BookmarkModel, NodeMoveObserver>>(
      node_move_observer_.get());
  model_observation_->Observe(model);
}

void BookmarkClientBase::AddSuggestedSaveLocationProvider(
    SuggestedSaveLocationProvider* suggestion_provider) {
  CHECK(suggestion_provider);
  save_location_providers_.push_back(suggestion_provider);
}

void BookmarkClientBase::RemoveSuggestedSaveLocationProvider(
    SuggestedSaveLocationProvider* suggestion_provider) {
  auto it = base::ranges::find(save_location_providers_, suggestion_provider);
  if (it != save_location_providers_.end()) {
    save_location_providers_.erase(it);
  }

  // Don't hold a removed provider since it may be deleted.
  if (last_used_provider_ == suggestion_provider) {
    last_used_provider_ = nullptr;
  }
}

const bookmarks::BookmarkNode* BookmarkClientBase::GetSuggestedSaveLocation(
    const GURL& url) {
  if (!bookmark_model_) {
    return nullptr;
  }

  const bookmarks::BookmarkNode* suggestion = nullptr;
  for (SuggestedSaveLocationProvider* provider : save_location_providers_) {
    std::string feature_name = provider->GetFeatureNameForMetrics();
    CHECK(!feature_name.empty());
    std::string histogram_name = kSaveLocationStateHistogramBase + feature_name;

    // If we found a suggestion, iterate over the other providers and report
    // that they were superseded.
    if (suggestion) {
      base::UmaHistogramEnumeration(
          histogram_name, provider->GetSuggestion(url)
                              ? SuggestedSaveLocationState::kSuperseded
                              : SuggestedSaveLocationState::kNoSuggestion);
      continue;
    }

    suggestion = provider->GetSuggestion(url);

    if (suggestion) {
      CHECK(suggestion->is_folder());

      auto it = temporarily_disallowed_suggestions_.find(suggestion->uuid());
      if (it != temporarily_disallowed_suggestions_.end()) {
        // Allow the suggestion if it was previously rejected and a certain
        // amount of time has passed.
        if (it->second < base::Time::Now()) {
          temporarily_disallowed_suggestions_.erase(suggestion->uuid());
        } else {
          base::UmaHistogramEnumeration(histogram_name,
                                        SuggestedSaveLocationState::kBlocked);
          suggestion = nullptr;
          continue;
        }
      }

      last_suggested_folder_uuid_ = suggestion->uuid();
      last_used_provider_ = provider;
      base::UmaHistogramEnumeration(histogram_name,
                                    SuggestedSaveLocationState::kUsed);
    } else {
      base::UmaHistogramEnumeration(histogram_name,
                                    SuggestedSaveLocationState::kNoSuggestion);
    }
  }

  if (suggestion) {
    last_suggested_save_time_ = base::Time::Now();
    return suggestion;
  }

  // If there was no suggestion, reset so we're not tracking for a "normal"
  // bookmark.
  last_suggested_folder_uuid_ = base::Uuid();
  last_used_provider_ = nullptr;

  // If there's no suggested folder, ensure we're not accidentally suggesting
  // one via the most recently modified. If the most recently modified folder
  // was recommended, find one that isn't.
  std::vector<const bookmarks::BookmarkNode*> recently_modified_list =
      bookmarks::GetMostRecentlyModifiedUserFolders(
          bookmark_model_, save_location_providers_.size() + 1);
  CHECK(!recently_modified_list.empty());

  std::string was_saved_to_suggested;
  int recently_modified_index = 0;
  bool found_value =
      recently_modified_list[recently_modified_index]->GetMetaInfo(
          kWasSuggestedFolderKey, &was_saved_to_suggested);
  bool most_recent_is_suggested = false;
  while (found_value && was_saved_to_suggested == kTrue) {
    most_recent_is_suggested = true;
    recently_modified_index++;
    found_value = recently_modified_list[recently_modified_index]->GetMetaInfo(
        kWasSuggestedFolderKey, &was_saved_to_suggested);
  }

  if (most_recent_is_suggested) {
    return recently_modified_list[recently_modified_index];
  }

  return nullptr;
}

BookmarkClientBase::NodeMoveObserver::NodeMoveObserver(
    BookmarkClientBase* client)
    : client_(client) {}

BookmarkClientBase::NodeMoveObserver::~NodeMoveObserver() = default;

void BookmarkClientBase::NodeMoveObserver::BookmarkModelChanged() {
  // noop
}

void BookmarkClientBase::NodeMoveObserver::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool newly_added) {
  bookmarks::BookmarkModel* model = client_->bookmark_model_;
  // Check to see if the saved bookmark actually used the suggested folder.
  if (parent->uuid() == client_->last_suggested_folder_uuid_) {
    // Consider the suggestion "accepted" until we see the bookmark moved to a
    // different location.
    model->SetNodeMetaInfo(parent, kWasSuggestedFolderKey, kTrue);

    // The suggestion was used, reset the state.
    client_->last_suggested_folder_uuid_ = base::Uuid();
  } else {
    // If it didn't, if the folder was suggested in the past but wasn't for
    // this save, it's an explicit save to the folder. This folder can now be
    // allowed in "recently used".
    std::string was_suggested;
    bool has_value =
        parent->GetMetaInfo(kWasSuggestedFolderKey, &was_suggested);
    if (has_value && was_suggested == kTrue) {
      model->SetNodeMetaInfo(parent, kWasSuggestedFolderKey, kFalse);
    }
  }
}

void BookmarkClientBase::NodeMoveObserver::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  bookmarks::BookmarkModel* model = client_->bookmark_model_;

  // If enough time has elapsed since the bookmark was saved, don't consider
  // the bookmark moving out of that folder to be a rejection.
  if (kRejectionCoolOffTime <
      base::Time::Now() - client_->last_suggested_save_time_) {
    return;
  }

  const bookmarks::BookmarkNode* node = new_parent->children()[new_index].get();

  // If the user changes the folder off of the suggested folder and it was the
  // last bookmark to be added, consider it rejected.
  std::vector<const bookmarks::BookmarkNode*> most_recent_nodes;
  GetMostRecentlyAddedEntries(model, 1, &most_recent_nodes);
  bool moved_most_recent_node = !most_recent_nodes.empty() &&
                                node->uuid() == most_recent_nodes[0]->uuid();
  std::string was_saved_to_suggested;
  old_parent->GetMetaInfo(kWasSuggestedFolderKey, &was_saved_to_suggested);
  if (moved_most_recent_node && was_saved_to_suggested == kTrue) {
    // Prevent the rejected folder's UUID from being suggested for some period
    // of time.
    base::TimeDelta backoff_time = base::Seconds(0);
    if (client_->last_used_provider_) {
      backoff_time = client_->last_used_provider_->GetBackoffTime();
      client_->last_used_provider_->OnSuggestionRejected();
    }
    client_->temporarily_disallowed_suggestions_[old_parent->uuid()] =
        base::Time::Now() + backoff_time;
  }

  // If the new parent of the bookmark has metadata indicating that it has been
  // suggested and the bookmark did _not_ start there, we consider this an
  // explicit save. From this point on, the folder is allowed to be listed as
  // the default folder.
  bool has_value =
      new_parent->GetMetaInfo(kWasSuggestedFolderKey, &was_saved_to_suggested);
  if (has_value && was_saved_to_suggested == kTrue) {
    model->SetNodeMetaInfo(new_parent, kWasSuggestedFolderKey, kFalse);
  }
}

}  // namespace power_bookmarks
