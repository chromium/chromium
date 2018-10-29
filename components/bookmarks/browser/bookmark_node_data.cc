// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node_data.h"

#include <string>

#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace bookmarks {

#if !defined(OS_MACOSX)
const char BookmarkNodeData::kClipboardFormatString[] =
    "chromium/x-bookmark-entries";
#endif

BookmarkNodeData::Element::Element() : is_url(false), id_(0) {
}

BookmarkNodeData::Element::Element(const BookmarkNode* node)
    : is_url(node->is_url()),
      url(node->url()),
      title(node->GetTitle()),
      date_added(node->date_added()),
      date_folder_modified(node->date_folder_modified()),
      id_(node->id()) {
  if (node->GetMetaInfoMap())
    meta_info_map = *node->GetMetaInfoMap();
  for (int i = 0; i < node->child_count(); ++i)
    children.push_back(Element(node->GetChild(i)));
}

BookmarkNodeData::Element::Element(const Element& other) = default;

BookmarkNodeData::Element::~Element() {
}

#if !defined(OS_MACOSX)
void BookmarkNodeData::Element::WriteToPickle(base::Pickle* pickle) const {
  pickle->WriteBool(is_url);
  pickle->WriteString(url.spec());
  pickle->WriteString16(title);
  pickle->WriteInt64(id_);
  pickle->WriteUInt32(static_cast<uint32_t>(meta_info_map.size()));
  for (auto it = meta_info_map.begin(); it != meta_info_map.end(); ++it) {
    pickle->WriteString(it->first);
    pickle->WriteString(it->second);
  }
  if (!is_url) {
    pickle->WriteUInt32(static_cast<uint32_t>(children.size()));
    for (auto i = children.begin(); i != children.end(); ++i) {
      i->WriteToPickle(pickle);
    }
  }
}

bool BookmarkNodeData::Element::ReadFromPickle(base::PickleIterator* iterator) {
  std::string url_spec;
  if (!iterator->ReadBool(&is_url) ||
      !iterator->ReadString(&url_spec) ||
      !iterator->ReadString16(&title) ||
      !iterator->ReadInt64(&id_)) {
    return false;
  }
  url = GURL(url_spec);
  date_added = base::Time();
  date_folder_modified = base::Time();
  meta_info_map.clear();
  uint32_t meta_field_count;
  if (!iterator->ReadUInt32(&meta_field_count))
    return false;
  for (size_t i = 0; i < meta_field_count; ++i) {
    std::string key;
    std::string value;
    if (!iterator->ReadString(&key) ||
        !iterator->ReadString(&value)) {
      return false;
    }
    meta_info_map[key] = value;
  }
  children.clear();
  if (!is_url) {
    uint32_t children_count;
    if (!iterator->ReadUInt32(&children_count))
      return false;
    children.reserve(children_count);
    for (size_t i = 0; i < children_count; ++i) {
      children.push_back(Element());
      if (!children.back().ReadFromPickle(iterator))
        return false;
    }
  }
  return true;
}
#endif

// BookmarkNodeData -----------------------------------------------------------

BookmarkNodeData::BookmarkNodeData() {
}

BookmarkNodeData::BookmarkNodeData(const BookmarkNodeData& other) = default;

BookmarkNodeData::BookmarkNodeData(const BookmarkNode* node) {
  elements.push_back(Element(node));
}

BookmarkNodeData::BookmarkNodeData(
    const std::vector<const BookmarkNode*>& nodes) {
  ReadFromVector(nodes);
}

BookmarkNodeData::~BookmarkNodeData() {
}

#if !defined(OS_MACOSX)
// static
bool BookmarkNodeData::ClipboardContainsBookmarks() {
  return ui::Clipboard::GetForCurrentThread()->IsFormatAvailable(
      ui::Clipboard::GetFormatType(kClipboardFormatString),
      ui::CLIPBOARD_TYPE_COPY_PASTE);
}
#endif

bool BookmarkNodeData::ReadFromVector(
    const std::vector<const BookmarkNode*>& nodes) {
  Clear();

  if (nodes.empty())
    return false;

  for (size_t i = 0; i < nodes.size(); ++i)
    elements.push_back(Element(nodes[i]));

  return true;
}

bool BookmarkNodeData::ReadFromTuple(const GURL& url,
                                     const base::string16& title) {
  Clear();

  if (!url.is_valid())
    return false;

  Element element;
  element.title = title;
  element.url = url;
  element.is_url = true;

  elements.push_back(element);

  return true;
}

#if !defined(OS_MACOSX)
void BookmarkNodeData::WriteToClipboard(ui::ClipboardType clipboard_type) {
  DCHECK(clipboard_type == ui::CLIPBOARD_TYPE_COPY_PASTE ||
         clipboard_type == ui::CLIPBOARD_TYPE_SELECTION);
  ui::ScopedClipboardWriter scw(clipboard_type);

#if defined(OS_WIN)
  const base::string16 kEOL(L"\r\n");
#else
  const base::string16 kEOL = base::ASCIIToUTF16("\n");
#endif

  // If there is only one element and it is a URL, write the URL to the
  // clipboard.
  if (has_single_url()) {
    const base::string16& title = elements[0].title;
    const std::string url = elements[0].url.spec();

    scw.WriteBookmark(title, url);

    // Don't call scw.WriteHyperlink() here, since some rich text editors will
    // change fonts when such data is pasted in; besides, most such editors
    // auto-linkify at some point anyway.

    // Also write the URL to the clipboard as text so that it can be pasted
    // into text fields. We use WriteText instead of WriteURL because we don't
    // want to clobber the X clipboard when the user copies out of the omnibox
    // on Linux (on Windows and Mac, there is no difference between these
    // functions).
    scw.WriteText(base::UTF8ToUTF16(url));
  } else {
    // We have either more than one URL, a folder, or a combination of URLs
    // and folders.
    base::string16 text;
    for (size_t i = 0; i < size(); i++) {
      text += i == 0 ? base::ASCIIToUTF16("") : kEOL;
      if (!elements[i].is_url) {
        // Then it's a folder. Only copy the name of the folder.
        const base::string16 title = elements[i].title;
        text += title;
      } else {
        const base::string16 url = base::UTF8ToUTF16(elements[i].url.spec());
        text += url;
      }
    }
    scw.WriteText(text);
  }

  base::Pickle pickle;
  WriteToPickle(base::FilePath(), &pickle);
  scw.WritePickledData(pickle,
                       ui::Clipboard::GetFormatType(kClipboardFormatString));
}

bool BookmarkNodeData::ReadFromClipboard(ui::ClipboardType type) {
  DCHECK_EQ(type, ui::CLIPBOARD_TYPE_COPY_PASTE);
  std::string data;
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  clipboard->ReadData(ui::Clipboard::GetFormatType(kClipboardFormatString),
                      &data);

  if (!data.empty()) {
    base::Pickle pickle(data.data(), static_cast<int>(data.size()));
    if (ReadFromPickle(&pickle))
      return true;
  }

  base::string16 title;
  std::string url;
  clipboard->ReadBookmark(&title, &url);
  if (!url.empty()) {
    Element element;
    element.is_url = true;
    element.url = GURL(url);
    element.title = title;

    elements.clear();
    elements.push_back(element);
    return true;
  }

  return false;
}

void BookmarkNodeData::WriteToPickle(const base::FilePath& profile_path,
                                     base::Pickle* pickle) const {
  profile_path.WriteToPickle(pickle);
  pickle->WriteUInt32(static_cast<uint32_t>(size()));

  for (size_t i = 0; i < size(); ++i)
    elements[i].WriteToPickle(pickle);
}

bool BookmarkNodeData::ReadFromPickle(base::Pickle* pickle) {
  base::PickleIterator data_iterator(*pickle);
  uint32_t element_count;
  if (profile_path_.ReadFromPickle(&data_iterator) &&
      data_iterator.ReadUInt32(&element_count)) {
    std::vector<Element> tmp_elements;
    tmp_elements.resize(element_count);
    for (size_t i = 0; i < element_count; ++i) {
      if (!tmp_elements[i].ReadFromPickle(&data_iterator)) {
        return false;
      }
    }
    elements.swap(tmp_elements);
  }

  return true;
}

#endif  // OS_MACOSX

std::vector<const BookmarkNode*> BookmarkNodeData::GetNodes(
    BookmarkModel* model,
    const base::FilePath& profile_path) const {
  std::vector<const BookmarkNode*> nodes;

  if (!IsFromProfilePath(profile_path))
    return nodes;

  for (size_t i = 0; i < size(); ++i) {
    const BookmarkNode* node = GetBookmarkNodeByID(model, elements[i].id_);
    if (!node) {
      nodes.clear();
      return nodes;
    }
    nodes.push_back(node);
  }
  return nodes;
}

const BookmarkNode* BookmarkNodeData::GetFirstNode(
    BookmarkModel* model,
    const base::FilePath& profile_path) const {
  std::vector<const BookmarkNode*> nodes = GetNodes(model, profile_path);
  return nodes.size() == 1 ? nodes[0] : nullptr;
}

void BookmarkNodeData::Clear() {
  profile_path_.clear();
  elements.clear();
}

void BookmarkNodeData::SetOriginatingProfilePath(
    const base::FilePath& profile_path) {
  DCHECK(profile_path_.empty());
  profile_path_ = profile_path;
}

bool BookmarkNodeData::IsFromProfilePath(
    const base::FilePath& profile_path) const {
  // An empty path means the data is not associated with any profile.
  return !profile_path_.empty() && profile_path_ == profile_path;
}

}  // namespace bookmarks
