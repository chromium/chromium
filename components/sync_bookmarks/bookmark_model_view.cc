// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_view.h"

#include "base/check.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/sync_bookmarks/initial_account_bookmark_deduplicator.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace sync_bookmarks {

namespace {

const bookmarks::BookmarkNode* GetAncestorPermanentFolder(
    const bookmarks::BookmarkNode* node) {
  CHECK(node);

  const bookmarks::BookmarkNode* self_or_ancestor = node;

  while (!self_or_ancestor->is_permanent_node()) {
    self_or_ancestor = self_or_ancestor->parent();
    CHECK(self_or_ancestor);
  }

  return self_or_ancestor;
}

}  // namespace

BookmarkModelView::BookmarkModelView(bookmarks::BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model->AsWeakPtr()) {
  CHECK(bookmark_model_);
}

BookmarkModelView::~BookmarkModelView() = default;

bool BookmarkModelView::IsNodeSyncable(
    const bookmarks::BookmarkNode* node) const {
  const bookmarks::BookmarkNode* ancestor_permanent_folder =
      GetAncestorPermanentFolder(node);
  CHECK(ancestor_permanent_folder);
  CHECK(ancestor_permanent_folder->is_permanent_node());
  CHECK_NE(ancestor_permanent_folder, root_node());

  // A node is considered syncable if it is a descendant of one of the syncable
  // permanent folder (e.g. excludes enterprise-managed nodes).
  return ancestor_permanent_folder == bookmark_bar_node() ||
         ancestor_permanent_folder == other_node() ||
         ancestor_permanent_folder == mobile_node();
}

bool BookmarkModelView::loaded() const {
  return bookmark_model_->loaded();
}

const bookmarks::BookmarkNode* BookmarkModelView::root_node() const {
  return bookmark_model_->root_node();
}

bool BookmarkModelView::is_permanent_node(
    const bookmarks::BookmarkNode* node) const {
  return bookmark_model_->is_permanent_node(node);
}

void BookmarkModelView::AddObserver(
    bookmarks::BookmarkModelObserver* observer) {
  bookmark_model_->AddObserver(observer);
}

void BookmarkModelView::RemoveObserver(
    bookmarks::BookmarkModelObserver* observer) {
  bookmark_model_->RemoveObserver(observer);
}

void BookmarkModelView::BeginExtensiveChanges() {
  bookmark_model_->BeginExtensiveChanges();
}

void BookmarkModelView::EndExtensiveChanges() {
  bookmark_model_->EndExtensiveChanges();
}

void BookmarkModelView::Remove(const bookmarks::BookmarkNode* node,
                               const base::Location& location) {
  bookmark_model_->Remove(node, bookmarks::metrics::BookmarkEditSource::kOther,
                          location);
}

void BookmarkModelView::Move(const bookmarks::BookmarkNode* node,
                             const bookmarks::BookmarkNode* new_parent,
                             size_t index) {
  bookmark_model_->Move(node, new_parent, index);
}

const gfx::Image& BookmarkModelView::GetFavicon(
    const bookmarks::BookmarkNode* node) {
  return bookmark_model_->GetFavicon(node);
}

void BookmarkModelView::SetTitle(const bookmarks::BookmarkNode* node,
                                 const std::u16string& title) {
  bookmark_model_->SetTitle(node, title,
                            bookmarks::metrics::BookmarkEditSource::kOther);
}

void BookmarkModelView::SetURL(const bookmarks::BookmarkNode* node,
                               const GURL& url) {
  bookmark_model_->SetURL(node, url,
                          bookmarks::metrics::BookmarkEditSource::kOther);
}

const bookmarks::BookmarkNode* BookmarkModelView::AddFolder(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const bookmarks::BookmarkNode::MetaInfoMap* meta_info,
    std::optional<base::Time> creation_time,
    std::optional<base::Uuid> uuid) {
  return bookmark_model_->AddFolder(parent, index, title, meta_info,
                                    creation_time, uuid);
}

const bookmarks::BookmarkNode* BookmarkModelView::AddURL(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url,
    const bookmarks::BookmarkNode::MetaInfoMap* meta_info,
    std::optional<base::Time> creation_time,
    std::optional<base::Uuid> uuid) {
  return bookmark_model_->AddURL(parent, index, title, url, meta_info,
                                 creation_time, uuid);
}

void BookmarkModelView::ReorderChildren(
    const bookmarks::BookmarkNode* parent,
    const std::vector<const bookmarks::BookmarkNode*>& ordered_nodes) {
  bookmark_model_->ReorderChildren(parent, ordered_nodes);
}

void BookmarkModelView::UpdateLastUsedTime(const bookmarks::BookmarkNode* node,
                                           const base::Time time,
                                           bool just_opened) {
  bookmark_model_->UpdateLastUsedTime(node, time, just_opened);
}

void BookmarkModelView::SetNodeMetaInfoMap(
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode::MetaInfoMap& meta_info_map) {
  bookmark_model_->SetNodeMetaInfoMap(node, meta_info_map);
}

BookmarkModelViewUsingLocalOrSyncableNodes::
    BookmarkModelViewUsingLocalOrSyncableNodes(
        bookmarks::BookmarkModel* bookmark_model)
    : BookmarkModelView(bookmark_model) {}

BookmarkModelViewUsingLocalOrSyncableNodes::
    ~BookmarkModelViewUsingLocalOrSyncableNodes() = default;

const bookmarks::BookmarkNode*
BookmarkModelViewUsingLocalOrSyncableNodes::bookmark_bar_node() const {
  return underlying_model()->bookmark_bar_node();
}

const bookmarks::BookmarkNode*
BookmarkModelViewUsingLocalOrSyncableNodes::other_node() const {
  return underlying_model()->other_node();
}

const bookmarks::BookmarkNode*
BookmarkModelViewUsingLocalOrSyncableNodes::mobile_node() const {
  return underlying_model()->mobile_node();
}

void BookmarkModelViewUsingLocalOrSyncableNodes::EnsurePermanentNodesExist() {
  // Local-or-syncable permanent folders always exist, nothing to be done.
  CHECK(bookmark_bar_node());
  CHECK(other_node());
  CHECK(mobile_node());
}

void BookmarkModelViewUsingLocalOrSyncableNodes::RemoveAllSyncableNodes() {
  underlying_model()->BeginExtensiveChanges();

  for (const auto& permanent_node : root_node()->children()) {
    if (!IsNodeSyncable(permanent_node.get())) {
      continue;
    }

    while (!permanent_node->children().empty()) {
      underlying_model()->RemoveLastChild(
          permanent_node.get(), bookmarks::metrics::BookmarkEditSource::kOther,
          FROM_HERE);
    }
  }

  underlying_model()->EndExtensiveChanges();
}

void BookmarkModelViewUsingLocalOrSyncableNodes::
    MaybeRemoveUnderlyingModelDuplicatesUponInitialSync() {}

const bookmarks::BookmarkNode*
BookmarkModelViewUsingLocalOrSyncableNodes::GetNodeByUuid(
    const base::Uuid& uuid) const {
  return underlying_model()->GetNodeByUuid(
      uuid,
      bookmarks::BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes);
}

BookmarkModelViewUsingAccountNodes::BookmarkModelViewUsingAccountNodes(
    bookmarks::BookmarkModel* bookmark_model)
    : BookmarkModelView(bookmark_model) {}

BookmarkModelViewUsingAccountNodes::~BookmarkModelViewUsingAccountNodes() =
    default;

const bookmarks::BookmarkNode*
BookmarkModelViewUsingAccountNodes::bookmark_bar_node() const {
  return underlying_model()->account_bookmark_bar_node();
}

const bookmarks::BookmarkNode* BookmarkModelViewUsingAccountNodes::other_node()
    const {
  return underlying_model()->account_other_node();
}

const bookmarks::BookmarkNode* BookmarkModelViewUsingAccountNodes::mobile_node()
    const {
  return underlying_model()->account_mobile_node();
}

void BookmarkModelViewUsingAccountNodes::EnsurePermanentNodesExist() {
  underlying_model()->CreateAccountPermanentFolders();
  CHECK(bookmark_bar_node());
  CHECK(other_node());
  CHECK(mobile_node());
}

void BookmarkModelViewUsingAccountNodes::RemoveAllSyncableNodes() {
  underlying_model()->RemoveAccountPermanentFolders();
}

void BookmarkModelViewUsingAccountNodes::
    MaybeRemoveUnderlyingModelDuplicatesUponInitialSync() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  BeginExtensiveChanges();

  const base::Time deduplication_start_time = base::Time::Now();

  InitialAccountBookmarkDeduplicator initial_account_bookmark_deduplicator(
      underlying_model());
  initial_account_bookmark_deduplicator.Deduplicate();

  const base::TimeDelta deduplication_duration =
      base::Time::Now() - deduplication_start_time;

  base::UmaHistogramCustomTimes("Sync.BookmarksSignInDeduplicationTime",
                                deduplication_duration,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Seconds(60),
                                /*buckets=*/50);

  EndExtensiveChanges();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

const bookmarks::BookmarkNode*
BookmarkModelViewUsingAccountNodes::GetNodeByUuid(
    const base::Uuid& uuid) const {
  return underlying_model()->GetNodeByUuid(
      uuid, bookmarks::BookmarkModel::NodeTypeForUuidLookup::kAccountNodes);
}

}  // namespace sync_bookmarks
