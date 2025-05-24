// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UI_OPERATIONS_HELPER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UI_OPERATIONS_HELPER_H_

#include <cstddef>

#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"

class BookmarkMergedSurfaceService;
struct BookmarkParentFolder;
class Profile;
class Browser;

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

  // Drops the bookmark nodes that are in `data` onto `target_parent()` at
  // `index`.
  // `copy` indicates the source operation: if true then the bookmarks in
  // `data` are copied, otherwise they are moved if they belong to the same
  // profile.
  // `browser` is needed if the user should be asked to confirm whether they
  // want to move a bookmark between local and account storage. Returns the drop
  // type used.
  ui::mojom::DragOperation DropBookmarks(
      Profile* profile,
      const bookmarks::BookmarkNodeData& data,
      size_t index,
      bool copy,
      chrome::BookmarkReorderDropTarget target,
      Browser* browser = nullptr);

  // Copies nodes onto the clipboard. The nodes are copied in such a way that if
  // pasted again new nodes can be created. Pass the calling context through as
  // `source`.
  static void CopyToClipboard(
      bookmarks::BookmarkModel* model,
      const std::vector<
          raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>& nodes,
      bookmarks::metrics::BookmarkEditSource source,
      bool is_off_the_record);

  // Copies nodes onto the clipboard then removes them from the bookmark model.
  // Pass the calling context through as `source`.
  static void CutToClipboard(
      bookmarks::BookmarkModel* model,
      const std::vector<
          raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>& nodes,
      bookmarks::metrics::BookmarkEditSource source,
      bool is_off_the_record);

  // Returns true if the user can paste from the clipboard a bookmark url/node
  // into `target_parent()`.
  bool CanPasteFromClipboard() const;

  // Pastes from the clipboard. The new nodes are added to `target_parent()`.
  // The nodes are inserted at `index`.
  void PasteFromClipboard(size_t index);

 protected:
  // Represents the target parent node for the operation.
  // For non-merged surfaces, it's a bookmark node.
  // For merged surfaces, it's either a bookmark node or a permanent folder.
  class TargetParent {
   public:
    virtual ~TargetParent();
    virtual bool IsManaged() const = 0;
    virtual bool IsPermanentNode() const = 0;
    virtual bool IsDirectChild(const bookmarks::BookmarkNode* node) const = 0;
    virtual bookmarks::BookmarkNode::Type GetType() const = 0;
    virtual const bookmarks::BookmarkNode* GetNodeAtIndex(
        size_t index) const = 0;
    virtual size_t GetChildrenCount() const = 0;
  };

  virtual bookmarks::BookmarkModel* model() = 0;
  virtual void AddNodesAsCopiesOfNodeData(
      const bookmarks::BookmarkNodeData& data,
      size_t index_to_add_at) = 0;
  virtual void MoveBookmarkNodeData(const bookmarks::BookmarkNodeData& data,
                                    const base::FilePath& profile_path,
                                    size_t index_to_add_at,
                                    Browser* browser) = 0;
  virtual const TargetParent* target_parent() const = 0;

 private:
  static void CopyOrCutToClipboard(
      bookmarks::BookmarkModel* model,
      const std::vector<
          raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>& nodes,
      bool remove_nodes,
      bookmarks::metrics::BookmarkEditSource source,
      bool is_off_the_record);

  // Updates `title` such that `url` and `title` pair are unique among the
  // children of `target_parent()`.
  void MakeTitleUnique(const GURL& url, std::u16string* title) const;
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
  void AddNodesAsCopiesOfNodeData(const bookmarks::BookmarkNodeData& data,
                                  size_t index_to_add_at) override;
  void MoveBookmarkNodeData(const bookmarks::BookmarkNodeData& data,
                            const base::FilePath& profile_path,
                            size_t index_to_add_at,
                            Browser* browser) override;
  const internal::BookmarkUIOperationsHelper::TargetParent* target_parent()
      const override;

 private:
  class TargetParent
      : public internal::BookmarkUIOperationsHelper::TargetParent {
   public:
    static std::unique_ptr<TargetParent> CreateTargetParent(
        bookmarks::BookmarkModel* model,
        const bookmarks::BookmarkNode* parent);

    TargetParent(const bookmarks::BookmarkNode* parent, bool is_managed);
    ~TargetParent() override;

    const bookmarks::BookmarkNode* parent_node() const;

    // internal::BookmarkUIOperationsHelper::TargetParent
    bool IsManaged() const override;
    bool IsPermanentNode() const override;
    bool IsDirectChild(const bookmarks::BookmarkNode* node) const override;
    bookmarks::BookmarkNode::Type GetType() const override;
    const bookmarks::BookmarkNode* GetNodeAtIndex(size_t index) const override;
    size_t GetChildrenCount() const override;

   private:
    const raw_ptr<const bookmarks::BookmarkNode> parent_;
    const bool is_managed_;
  };

  const bookmarks::BookmarkNode* parent_node() const;

  const raw_ptr<bookmarks::BookmarkModel> model_;
  const std::unique_ptr<TargetParent> target_parent_;
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

  // Merged bookmark surfaces can trigger a non-merged bookmark UI e.g. bookmark
  // context menu can trigger the edit bookmark UI or the bookmark manager with
  // a choice of a highlighted node.
  // This function returns the default parent to use for non-merged surfaces UIs
  // triggered from merged surfaces.
  // This function treats managed nodes as some operations are allowed; e.g.
  // opening the bookmark manager with a managed node highlighted. This should
  // not be used by managed folders if intended to be used for adding new nodes,
  // since managed bookmarks do not support creating nodes.
  const bookmarks::BookmarkNode* GetDefaultParentForNonMergedSurfaces() const;

 protected:
  bookmarks::BookmarkModel* model() override;
  void AddNodesAsCopiesOfNodeData(const bookmarks::BookmarkNodeData& data,
                                  size_t index_to_add_at) override;
  void MoveBookmarkNodeData(const bookmarks::BookmarkNodeData& data,
                            const base::FilePath& profile_path,
                            size_t index_to_add_at,
                            Browser* browser) override;
  const internal::BookmarkUIOperationsHelper::TargetParent* target_parent()
      const override;

 private:
  class TargetParent
      : public internal::BookmarkUIOperationsHelper::TargetParent {
   public:
    static std::unique_ptr<TargetParent> CreateTargetParent(
        BookmarkMergedSurfaceService* merged_surface_service,
        const BookmarkParentFolder* parent);

    TargetParent(BookmarkMergedSurfaceService* merged_surface_service,
                 const BookmarkParentFolder* parent);
    ~TargetParent() override;

    const BookmarkParentFolder* parent_folder() const;

    // internal::BookmarkUIOperationsHelper::TargetParent
    bool IsManaged() const override;
    bool IsPermanentNode() const override;
    bool IsDirectChild(const bookmarks::BookmarkNode* node) const override;
    bookmarks::BookmarkNode::Type GetType() const override;
    const bookmarks::BookmarkNode* GetNodeAtIndex(size_t index) const override;
    size_t GetChildrenCount() const override;

   private:
    const raw_ptr<BookmarkMergedSurfaceService> merged_surface_service_;
    const raw_ptr<const BookmarkParentFolder> parent_;
  };

  const BookmarkParentFolder* parent_folder() const;

  const raw_ptr<BookmarkMergedSurfaceService> merged_surface_service_;
  const std::unique_ptr<TargetParent> target_parent_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_UI_OPERATIONS_HELPER_H_
