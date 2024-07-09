// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_CORE_BOOKMARK_CLIENT_BASE_H_
#define COMPONENTS_POWER_BOOKMARKS_CORE_BOOKMARK_CLIENT_BASE_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/power_bookmarks/core/suggested_save_location_provider.h"

namespace base {
class Time;
class Uuid;
}  // namespace base

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace power_bookmarks {

extern const char kSaveLocationStateHistogramBase[];

// The possible ways a suggested save location can be handled. These must be
// kept in sync with the values in enums.xml.
enum class SuggestedSaveLocationState {
  // The suggestion provider did not have a suggestion.
  kNoSuggestion = 0,

  // The provider had a suggestion but it was blocked from a prior rejection.
  kBlocked = 1,

  // The suggestion was used to create the bookmark.
  kUsed = 2,

  // The provider had a suggestion but was superseded by some other feature.
  kSuperseded = 3,

  // This enum must be last and is only used for histograms.
  kMaxValue = kSuperseded
};

// The amount of time between a save to a suggested folder and a move out of
// that folder that the suggested folder will be considered rejected.
extern const base::TimeDelta kRejectionCoolOffTime;

class BookmarkClientBase : public bookmarks::BookmarkClient {
 public:
  BookmarkClientBase();
  BookmarkClientBase(const BookmarkClientBase&) = delete;
  BookmarkClientBase& operator=(const BookmarkClientBase&) = delete;
  ~BookmarkClientBase() override;

  void Init(bookmarks::BookmarkModel* model) override;

  const bookmarks::BookmarkNode* GetSuggestedSaveLocation(
      const GURL& url) override;

  // Allow features to suggest a location to save to. Features should retain
  // ownership of their respective providers.
  void AddSuggestedSaveLocationProvider(
      SuggestedSaveLocationProvider* suggestion_provider);
  void RemoveSuggestedSaveLocationProvider(
      SuggestedSaveLocationProvider* suggestion_provider);

 private:
  class NodeMoveObserver : public bookmarks::BaseBookmarkModelObserver {
   public:
    explicit NodeMoveObserver(BookmarkClientBase* client);
    ~NodeMoveObserver() override;

    void BookmarkModelChanged() override;
    void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                           size_t index,
                           bool newly_added) override;
    void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* new_parent,
                           size_t new_index) override;

   private:
    // A handle to the owning object. This pointer will always outlive this
    // object.
    raw_ptr<BookmarkClientBase> client_;
  };

  std::unique_ptr<NodeMoveObserver> node_move_observer_;
  std::unique_ptr<
      base::ScopedObservation<bookmarks::BookmarkModel, NodeMoveObserver>>
      model_observation_{};
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_{nullptr};

  // A list of providers of a save location for a given URL.
  std::vector<raw_ptr<SuggestedSaveLocationProvider>> save_location_providers_;

  // The UUID of the last folder that was suggested.
  base::Uuid last_suggested_folder_uuid_;

  // The time that the last save to a suggested folder occurred.
  base::Time last_suggested_save_time_;

  // The last provider that was used to pass a suggestion to a feature.
  raw_ptr<SuggestedSaveLocationProvider> last_used_provider_{nullptr};

  // A map of suggestions that were rejected by the user to the time they will
  // next be allowed to suggested. This is intentionally in-memory only,
  // restarting the browser will allow all suggestions again.
  std::map<base::Uuid, base::Time> temporarily_disallowed_suggestions_;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_CORE_BOOKMARK_CLIENT_BASE_H_
