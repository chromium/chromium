// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UI_OPERATIONS_HELPER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UI_OPERATIONS_HELPER_H_

#include <cstddef>

#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"

class BookmarkMergedSurfaceService;
struct BookmarkParentFolder;
class Profile;

namespace base {
class FilePath;
}

namespace bookmarks {
class BookmarkModel;
struct BookmarkNodeData;
}  // namespace bookmarks

namespace chrome {
enum class BookmarkReorderDropTarget;
}

namespace internal {

class BookmarkUIOperationsHelper {
 public:
  virtual ~BookmarkUIOperationsHelper();

  // Drops the bookmark nodes that are in |data| onto |parent_node| at
  // |index|.
  // |copy| indicates the source operation: if true then the bookmarks in
  // |data| are copied, otherwise they are moved if they belong to the same
  // |profile|. Returns the drop type used.
  ui::mojom::DragOperation DropBookmarks(
      Profile* profile,
      const bookmarks::BookmarkNodeData& data,
      size_t index,
      bool copy,
      chrome::BookmarkReorderDropTarget target);

 protected:
  virtual bookmarks::BookmarkModel* model() = 0;
  virtual void CopyBookmarkNodeData(const bookmarks::BookmarkNodeData& data,
                                    const base::FilePath& profile_path,
                                    size_t index_to_add_at) = 0;
  virtual void MoveBookmarkNodeData(const bookmarks::BookmarkNodeData& data,
                                    const base::FilePath& profile_path,
                                    size_t index_to_add_at) = 0;

  virtual bool IsParentManaged() const = 0;
  virtual bool IsParentPermanentNode() const = 0;
  virtual bool IsParentDirectChild(
      const bookmarks::BookmarkNode* node) const = 0;
  virtual bookmarks::BookmarkNode::Type GetParentType() const = 0;
};

}  // namespace internal

// Helper class for UI operations e.g. Drop, Copy, Paste bookmarks.
//  `BookmarkUIOperationsHelperNonMergedSurfaces` must be used in non bookmark
//  merged surfaces where there is a clear distinction between account and local
//  bookmarks.
class BookmarkUIOperationsHelperNonMergedSurfaces
    : public internal::BookmarkUIOperationsHelper {
 public:
  // `model` and `parent` must be not null and outlive `this`.
  // `parent` is the parent node where nodes are being dropped, copy/cut ->
  // paste into parent folder.
  BookmarkUIOperationsHelperNonMergedSurfaces(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* parent);

  BookmarkUIOperationsHelperNonMergedSurfaces(
      const BookmarkUIOperationsHelperNonMergedSurfaces&) = delete;
  BookmarkUIOperationsHelperNonMergedSurfaces& operator=(
      const BookmarkUIOperationsHelperNonMergedSurfaces&) = delete;

  ~BookmarkUIOperationsHelperNonMergedSurfaces() override;

 protected:
  bookmarks::BookmarkModel* model() override;
  void CopyBookmarkNodeData(const bookmarks::BookmarkNodeData& data,
                            const base::FilePath& profile_path,
                            size_t index_to_add_at) override;
  void MoveBookmarkNodeData(const bookmarks::BookmarkNodeData& data,
                            const base::FilePath& profile_path,
                            size_t index_to_add_at) override;

  bool IsParentManaged() const override;
  bool IsParentPermanentNode() const override;
  bool IsParentDirectChild(const bookmarks::BookmarkNode* node) const override;
  bookmarks::BookmarkNode::Type GetParentType() const override;

 private:
  const raw_ptr<bookmarks::BookmarkModel> model_;
  const raw_ptr<const bookmarks::BookmarkNode> parent_;
};

// Helper class for UI operations e.g. Drop, Copy, Paste bookmarks.
//  `BookmarkUIOperationsHelperNonMergedSurfaces` must be used in bookmark
//  merged surfaces (Bookmark bar, bookmark menu and side panel).
class BookmarkUIOperationsHelperMergedSurfaces
    : public internal::BookmarkUIOperationsHelper {
 public:
  BookmarkUIOperationsHelperMergedSurfaces(
      BookmarkMergedSurfaceService* merged_surface_service,
      const BookmarkParentFolder* parent);

  BookmarkUIOperationsHelperMergedSurfaces(
      const BookmarkUIOperationsHelperMergedSurfaces&) = delete;
  BookmarkUIOperationsHelperMergedSurfaces& operator=(
      const BookmarkUIOperationsHelperMergedSurfaces&) = delete;

  ~BookmarkUIOperationsHelperMergedSurfaces() override;

 protected:
  bookmarks::BookmarkModel* model() override;
  void CopyBookmarkNodeData(const bookmarks::BookmarkNodeData& data,
                            const base::FilePath& profile_path,
                            size_t index_to_add_at) override;
  void MoveBookmarkNodeData(const bookmarks::BookmarkNodeData& data,
                            const base::FilePath& profile_path,
                            size_t index_to_add_at) override;

  bool IsParentManaged() const override;
  bool IsParentPermanentNode() const override;
  bool IsParentDirectChild(const bookmarks::BookmarkNode* node) const override;
  bookmarks::BookmarkNode::Type GetParentType() const override;

 private:
  const raw_ptr<BookmarkMergedSurfaceService> merged_surface_service_;
  const raw_ptr<const BookmarkParentFolder> parent_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UI_OPERATIONS_HELPER_H_
