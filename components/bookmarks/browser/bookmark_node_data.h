// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_NODE_DATA_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_NODE_DATA_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "url/gurl.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/base/clipboard/clipboard_format_type.h"
#endif

namespace base {
class Pickle;
class PickleIterator;
}

#if defined(TOOLKIT_VIEWS)
namespace ui {
class OSExchangeData;
}
#endif

namespace bookmarks {

class BookmarkModel;

// BookmarkNodeData is used to represent the following:
//
// . A single URL.
// . A single node from the bookmark model.
// . A set of nodes from the bookmark model.
//
// BookmarkNodeData is used by bookmark related views to represent a dragged
// bookmark or bookmarks.
//
// Typical usage when writing data for a drag is:
//   BookmarkNodeData data(node_user_is_dragging);
//   data.Write(os_exchange_data_for_drag);
//
// Typical usage to read is:
//   BookmarkNodeData data;
//   if (data.Read(os_exchange_data))
//     // data is valid, contents are in elements.

struct BookmarkNodeData {
  // Element represents a single node.
  struct Element {
    Element();
    explicit Element(const BookmarkNode* node);
    Element(const Element& other);
    ~Element();

    // If true, this element represents a URL.
    bool is_url;

    // The URL, only valid if is_url is true.
    GURL url;

    // Title of the entry, used for both urls and folders.
    std::u16string title;

    // Date of when this node was created.
    base::Time date_added;

    // Date of the last modification. Only used for folders.
    base::Time date_folder_modified;

    // Children, only used for non-URL nodes.
    std::vector<Element> children;

    // Meta info for the bookmark node.
    BookmarkNode::MetaInfoMap meta_info_map;

    int64_t id() const { return id_; }

   private:
    friend struct BookmarkNodeData;

#if !BUILDFLAG(IS_APPLE)
    // For reading/writing this Element.
    void WriteToPickle(base::Pickle* pickle) const;
    bool ReadFromPickle(base::PickleIterator* iterator);
#endif

    // ID of the node.
    int64_t id_;
  };

#if !BUILDFLAG(IS_APPLE)
  // The MIME type for the clipboard format for BookmarkNodeData. This type is
  // not used on the Mac.
  static const char kClipboardFormatString[];
#endif

  BookmarkNodeData();
  BookmarkNodeData(const BookmarkNodeData& other);

  // Created a BookmarkNodeData populated from the arguments.
  explicit BookmarkNodeData(const BookmarkNode* node);
  explicit BookmarkNodeData(
      const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
          nodes);

  ~BookmarkNodeData();

#if defined(TOOLKIT_VIEWS)
  static const ui::ClipboardFormatType& GetBookmarkFormatType();
#endif

  static bool ClipboardContainsBookmarks();

  // Reads bookmarks from the given vector.
  bool ReadFromVector(
      const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
          nodes);

  // Creates a single-bookmark DragData from url/title pair.
  bool ReadFromTuple(const GURL& url, const std::u16string& title);

  // Writes bookmarks to the specified clipboard.
  void WriteToClipboard(bool is_off_the_record);

  // Reads bookmarks from the specified clipboard. Prefers data written via
  // WriteToClipboard() but will also attempt to read a plain bookmark.
  bool ReadFromClipboard(ui::ClipboardBuffer buffer);

#if defined(TOOLKIT_VIEWS)
  // Writes elements to data. If there is only one element and it is a URL
  // the URL and title are written to the clipboard in a format other apps can
  // use.
  // |profile_path| is used to identify which profile the data came from. Use an
  // empty path to indicate that the data is not associated with any profile.
  void Write(const base::FilePath& profile_path,
             ui::OSExchangeData* data) const;

  // Restores this data from the clipboard, returning true on success.
  bool Read(const ui::OSExchangeData& data);
#endif

#if !BUILDFLAG(IS_APPLE)
  // Writes the data for a drag to |pickle|.
  void WriteToPickle(const base::FilePath& profile_path,
                     base::Pickle* pickle) const;

  // Reads the data for a drag from a |pickle|.
  bool ReadFromPickle(base::Pickle* pickle);
#endif

  // Returns the nodes represented by this DragData. If this DragData was
  // created from the same profile then the nodes from the model are returned.
  // If the nodes can't be found (may have been deleted), an empty vector is
  // returned.
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> GetNodes(
      BookmarkModel* model,
      const base::FilePath& profile_path) const;

  // Convenience for getting the first node. Returns NULL if the data doesn't
  // match any nodes or there is more than one node.
  const BookmarkNode* GetFirstNode(BookmarkModel* model,
                                   const base::FilePath& profile_path) const;

  // Do we contain valid data?
  bool is_valid() const { return !elements.empty(); }

  // Returns true if there is a single url.
  bool has_single_url() const { return size() == 1 && elements[0].is_url; }

  // Number of elements.
  size_t size() const { return elements.size(); }

  // Clears the data.
  void Clear();

  // Sets |profile_path_|. This is useful for the constructors/readers that
  // don't set it. This should only be called if the profile path is not
  // already set.
  void SetOriginatingProfilePath(const base::FilePath& profile_path);

  // Returns true if this data is from the specified profile path.
  bool IsFromProfilePath(const base::FilePath& profile_path) const;

  // The actual elements written to the clipboard.
  std::vector<Element> elements;

 private:
  // Path of the profile we originated from.
  base::FilePath profile_path_;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_NODE_DATA_H_
