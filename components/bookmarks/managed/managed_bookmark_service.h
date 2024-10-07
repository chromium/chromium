// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARK_SERVICE_H_
#define COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARK_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace bookmarks {

class BookmarkModel;
class ManagedBookmarksTracker;

// ManagedBookmarkService manages the bookmark folder controlled by enterprise
// policy.
class ManagedBookmarkService : public KeyedService,
                               public BaseBookmarkModelObserver {
 public:
  using GetManagementDomainCallback = base::RepeatingCallback<std::string()>;

  ManagedBookmarkService(PrefService* prefs,
                         GetManagementDomainCallback callback);

  ManagedBookmarkService(const ManagedBookmarkService&) = delete;
  ManagedBookmarkService& operator=(const ManagedBookmarkService&) = delete;

  ~ManagedBookmarkService() override;

  // Called upon creation of the BookmarkModel.
  void BookmarkModelCreated(BookmarkModel* bookmark_model);

  // Returns a task that will be used to load a managed root node. This task
  // will be invoked in the Profile's IO task runner.
  LoadManagedNodeCallback GetLoadManagedNodeCallback();

  // Returns true if the |node| can have its title updated.
  bool CanSetPermanentNodeTitle(const BookmarkNode* node);

  // Returns true if |node| is a descendant of the managed node.
  bool IsNodeManaged(const BookmarkNode* node);

  // Top-level managed bookmarks folder, defined by an enterprise policy; may be
  // null.
  const BookmarkNode* managed_node() const { return managed_node_; }

 private:
  // KeyedService implementation.
  void Shutdown() override;

  // BaseBookmarkModelObserver implementation.
  void BookmarkModelChanged() override;

  // BookmarkModelObserver implementation.
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;

  // Cleanup, called when service is shutdown or when BookmarkModel is being
  // destroyed.
  void Cleanup();

  // Pointer to the PrefService. Must outlive ManagedBookmarkService.
  raw_ptr<PrefService> prefs_;

  // Pointer to the BookmarkModel; may be null. Only valid between the calls to
  // BookmarkModelCreated() and to BookmarkModelBeingDestroyed().
  raw_ptr<BookmarkModel> bookmark_model_;

  // Observation for the bookmark_model_
  base::ScopedObservation<BookmarkModel, BaseBookmarkModelObserver>
      bookmark_model_observation_{this};

  // Managed bookmarks are defined by an enterprise policy. The lifetime of the
  // BookmarkPermanentNode is controlled by BookmarkModel.
  std::unique_ptr<ManagedBookmarksTracker> managed_bookmarks_tracker_;
  GetManagementDomainCallback managed_domain_callback_;
  raw_ptr<BookmarkPermanentNode> managed_node_;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARK_SERVICE_H_
