// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARK_SERVICE_H_
#define COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARK_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_storage.h"
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
  typedef base::Callback<std::string()> GetManagementDomainCallback;

  ManagedBookmarkService(PrefService* prefs,
                         const GetManagementDomainCallback& callback);
  ~ManagedBookmarkService() override;

  // Called upon creation of the BookmarkModel.
  void BookmarkModelCreated(BookmarkModel* bookmark_model);

  // Returns a task that will be used to load a managed root node. This task
  // will be invoked in the Profile's IO task runner.
  LoadManagedNodeCallback GetLoadManagedNodeCallback();

  // Returns true if the |node| can have its title updated.
  bool CanSetPermanentNodeTitle(const BookmarkNode* node);

  // Returns true if |node| should sync.
  bool CanSyncNode(const BookmarkNode* node);

  // Returns true if |node| can be edited by the user.
  // TODO(joaodasilva): the model should check this more aggressively, and
  // should give the client a means to temporarily disable those checks.
  // http://crbug.com/49598
  bool CanBeEditedByUser(const BookmarkNode* node);

  // Top-level managed bookmarks folder, defined by an enterprise policy; may be
  // null.
  const BookmarkNode* managed_node() { return managed_node_; }

 private:
  // KeyedService implementation.
  void Shutdown() override;

  // BaseBookmarkModelObserver implementation.
  void BookmarkModelChanged() override;

  // BookmarkModelObserver implementation.
  void BookmarkModelLoaded(BookmarkModel* bookmark_model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(BookmarkModel* bookmark_model) override;

  // Cleanup, called when service is shutdown or when BookmarkModel is being
  // destroyed.
  void Cleanup();

  // Pointer to the PrefService. Must outlive ManagedBookmarkService.
  PrefService* prefs_;

  // Pointer to the BookmarkModel; may be null. Only valid between the calls to
  // BookmarkModelCreated() and to BookmarkModelBeingDestroyed().
  BookmarkModel* bookmark_model_;

  // Managed bookmarks are defined by an enterprise policy. The lifetime of the
  // BookmarkPermanentNode is controlled by BookmarkModel.
  std::unique_ptr<ManagedBookmarksTracker> managed_bookmarks_tracker_;
  GetManagementDomainCallback managed_domain_callback_;
  BookmarkPermanentNode* managed_node_;

  DISALLOW_COPY_AND_ASSIGN(ManagedBookmarkService);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARK_SERVICE_H_
