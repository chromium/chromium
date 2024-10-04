// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/managed/managed_bookmark_service.h"

#include <stdint.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/managed/managed_bookmarks_tracker.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace bookmarks {
namespace {

// BookmarkPermanentNodeLoader initializes a BookmarkPermanentNode from a JSON
// representation, title id and starting node id.
class BookmarkPermanentNodeLoader {
 public:
  BookmarkPermanentNodeLoader(std::unique_ptr<BookmarkPermanentNode> node,
                              base::Value::List initial_bookmarks,
                              int title_id)
      : node_(std::move(node)),
        initial_bookmarks_(std::move(initial_bookmarks)),
        title_id_(title_id) {
    DCHECK(node_);
  }

  BookmarkPermanentNodeLoader(const BookmarkPermanentNodeLoader&) = delete;
  BookmarkPermanentNodeLoader& operator=(const BookmarkPermanentNodeLoader&) =
      delete;

  ~BookmarkPermanentNodeLoader() = default;

  // Initializes |node_| from |initial_bookmarks_| and |title_id_| and returns
  // it. The ids are assigned starting at |next_node_id| and the value is
  // updated as a side-effect.
  std::unique_ptr<BookmarkPermanentNode> Load(int64_t* next_node_id) {
    node_->set_id(*next_node_id);
    *next_node_id = ManagedBookmarksTracker::LoadInitial(
        node_.get(), initial_bookmarks_, node_->id() + 1);
    node_->SetTitle(l10n_util::GetStringUTF16(title_id_));
    return std::move(node_);
  }

 private:
  std::unique_ptr<BookmarkPermanentNode> node_;
  base::Value::List initial_bookmarks_;
  int title_id_;
};

// Returns a std::unique_ptr<BookmarkPermanentNode> using |next_node_id| for
// assigning an id. |next_node_id| is updated as a side effect of calling this
// method.
std::unique_ptr<BookmarkPermanentNode> LoadManagedNode(
    std::unique_ptr<BookmarkPermanentNodeLoader> loader,
    int64_t* next_node_id) {
  return loader->Load(next_node_id);
}

}  // namespace

ManagedBookmarkService::ManagedBookmarkService(
    PrefService* prefs,
    GetManagementDomainCallback callback)
    : prefs_(prefs),
      bookmark_model_(nullptr),
      managed_domain_callback_(std::move(callback)),
      managed_node_(nullptr) {}

ManagedBookmarkService::~ManagedBookmarkService() {
  DCHECK(!bookmark_model_);
}

void ManagedBookmarkService::BookmarkModelCreated(
    BookmarkModel* bookmark_model) {
  DCHECK(bookmark_model);
  DCHECK(!bookmark_model_);
  bookmark_model_ = bookmark_model;
  bookmark_model_observation_.Observe(bookmark_model_);

  managed_bookmarks_tracker_ = std::make_unique<ManagedBookmarksTracker>(
      bookmark_model_, prefs_, managed_domain_callback_);
}

LoadManagedNodeCallback ManagedBookmarkService::GetLoadManagedNodeCallback() {
  // Create a BookmarkPermanentNode with a temporary id of 0. It will be
  // populated and assigned a proper id in the LoadManagedNode callback. Until
  // then, it is owned by the returned closure.
  std::unique_ptr<BookmarkPermanentNode> managed =
      BookmarkPermanentNode::CreateManagedBookmarks(/*id=*/0);

  managed_node_ = managed.get();

  auto loader = std::make_unique<BookmarkPermanentNodeLoader>(
      std::move(managed),
      managed_bookmarks_tracker_->GetInitialManagedBookmarks(),
      IDS_BOOKMARK_BAR_MANAGED_FOLDER_DEFAULT_NAME);

  return base::BindOnce(&LoadManagedNode, std::move(loader));
}

bool ManagedBookmarkService::CanSetPermanentNodeTitle(
    const BookmarkNode* node) {
  // |managed_node_| can have its title updated if the user signs in or out,
  // since the name of the managed domain can appear in it. It can also have
  // its title updated on locale changes (http://crbug.com/459448).
  if (node == managed_node_)
    return true;
  return !IsDescendantOf(node, managed_node_);
}

bool ManagedBookmarkService::IsNodeManaged(const BookmarkNode* node) {
  return IsDescendantOf(node, managed_node_);
}

void ManagedBookmarkService::Shutdown() {
  Cleanup();
}

void ManagedBookmarkService::BookmarkModelChanged() {}

void ManagedBookmarkService::BookmarkModelLoaded(bool ids_reassigned) {
  BaseBookmarkModelObserver::BookmarkModelLoaded(ids_reassigned);
  // Start tracking the managed bookmarks. This will detect any changes that may
  // have occurred while the initial managed bookmarks were being loaded on the
  // background.
  managed_bookmarks_tracker_->Init(managed_node_);
}

void ManagedBookmarkService::BookmarkModelBeingDeleted() {
  Cleanup();
}

void ManagedBookmarkService::Cleanup() {
  bookmark_model_observation_.Reset();
  if (bookmark_model_) {
    bookmark_model_ = nullptr;
  }

  managed_bookmarks_tracker_.reset();

  managed_node_ = nullptr;
}

}  // namespace bookmarks
