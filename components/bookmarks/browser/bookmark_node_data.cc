// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node_data.h"

#include <algorithm>
#include <string>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace bookmarks {

#if !BUILDFLAG(IS_APPLE)
namespace {
constexpr size_t kMaxVectorPreallocateSize = 10000;
}  // namespace

const char BookmarkNodeData::kClipboardFormatString[] =
    "chromium/x-bookmark-entries";
#endif

BookmarkNodeData::Element::Element() : is_url(false), id_(0) {}

BookmarkNodeData::Element::Element(const BookmarkNode* node)
    : is_url(node->is_url()),
      url(node->url()),
      title(node->GetTitle()),
      date_added(node->date_added()),
      date_folder_modified(node->date_folder_modified()),
      id_(node->id()) {
  if (node->GetMetaInfoMap()) {
    meta_info_map = *node->GetMetaInfoMap();
  }
  for (const auto& child : node->children()) {
    children.emplace_back(child.get());
  }
}

BookmarkNodeData::Element::Element(const Element& other) = default;

BookmarkNodeData::Element::~Element() {}

#if !BUILDFLAG(IS_APPLE)
void BookmarkNodeData::Element::WriteToLegacyPickle(
    base::Pickle* pickle) const {
  pickle->WriteBool(is_url);
  pickle->WriteString(url.spec());
  pickle->WriteString16(title);
  pickle->WriteInt64(id_);
  pickle->WriteUInt32(static_cast<uint32_t>(meta_info_map.size()));
  for (const auto& [key, value] : meta_info_map) {
    pickle->WriteString(key);
    pickle->WriteString(value);
  }
  if (!is_url) {
    pickle->WriteUInt32(static_cast<uint32_t>(children.size()));
    for (const auto& child : children) {
      child.WriteToLegacyPickle(pickle);
    }
  }
}

bool BookmarkNodeData::Element::ReadFromLegacyPickle(
    base::PickleIterator* iterator) {
  std::string url_spec;
  if (!iterator->ReadBool(&is_url) || !iterator->ReadString(&url_spec) ||
      !iterator->ReadString16(&title) || !iterator->ReadInt64(&id_)) {
    return false;
  }
  url = GURL(url_spec);
  date_added = base::Time();
  date_folder_modified = base::Time();
  meta_info_map.clear();
  uint32_t meta_field_count;
  if (!iterator->ReadUInt32(&meta_field_count)) {
    return false;
  }
  for (size_t i = 0; i < meta_field_count; ++i) {
    std::string key;
    std::string value;
    if (!iterator->ReadString(&key) || !iterator->ReadString(&value)) {
      return false;
    }
    meta_info_map[key] = value;
  }
  children.clear();
  if (!is_url) {
    uint32_t children_count_tmp;
    if (!iterator->ReadUInt32(&children_count_tmp)) {
      return false;
    }
    if (!base::IsValueInRangeForNumericType<size_t>(children_count_tmp)) {
      LOG(WARNING) << "children_count failed bounds check";
      return false;
    }
    // Note: do not preallocate the children vector. A pickle could be
    // constructed to contain N nested Elements. By continually recursing on
    // this ReadFromPickle function, the fast-fail logic is subverted. Each
    // child would claim it contains M more children. The first (and only) child
    // provided would claim M more children. We would allocate N * M Elements
    // along the way, while only receiving N Elements.
    const size_t children_count =
        base::checked_cast<size_t>(children_count_tmp);
    for (size_t i = 0; i < children_count; ++i) {
      children.emplace_back();
      if (!children.back().ReadFromLegacyPickle(iterator)) {
        return false;
      }
    }
  }
  return true;
}

base::Pickle BookmarkNodeData::Element::ToPickle() const {
  base::Pickle pickle;
  pickle.WriteBool(is_url);
  pickle.WriteString(url.spec());
  pickle.WriteString16(title);
  pickle.WriteInt64(id_);
  pickle.WriteUInt32(static_cast<uint32_t>(meta_info_map.size()));

  for (const auto& [key, value] : meta_info_map) {
    pickle.WriteString(key);
    pickle.WriteString(value);
  }

  if (!is_url) {
    pickle.WriteUInt32(static_cast<uint32_t>(children.size()));
    for (const auto& child : children) {
      pickle.WriteData(child.ToPickle());
    }
  }
  return pickle;
}

bool BookmarkNodeData::Element::FromPickle(base::PickleIterator iterator) {
  std::string url_spec;
  if (!iterator.ReadBool(&is_url) || !iterator.ReadString(&url_spec) ||
      !iterator.ReadString16(&title) || !iterator.ReadInt64(&id_)) {
    return false;
  }

  url = GURL(url_spec);
  date_added = base::Time();
  date_folder_modified = base::Time();
  meta_info_map.clear();

  uint32_t meta_field_count = 0;
  if (!iterator.ReadUInt32(&meta_field_count)) {
    return false;
  }

  for (size_t i = 0; i < meta_field_count; ++i) {
    std::string key;
    std::string value;
    if (!iterator.ReadString(&key) || !iterator.ReadString(&value)) {
      return false;
    }
    meta_info_map[key] = value;
  }

  children.clear();
  if (!is_url) {
    uint32_t children_count_tmp = 0;
    if (!iterator.ReadUInt32(&children_count_tmp)) {
      return false;
    }

    if (!base::IsValueInRangeForNumericType<size_t>(children_count_tmp)) {
      LOG(WARNING) << "children_count failed bounds check";
      return false;
    }
    // Note: do not preallocate the children vector. A pickle could be
    // constructed to contain N nested Elements. By continually recursing on
    // this ReadFromPickle function, the fast-fail logic is subverted. Each
    // child would claim it contains M more children. The first (and only) child
    // provided would claim M more children. We would allocate N * M Elements
    // along the way, while only receiving N Elements.
    const size_t children_count =
        base::checked_cast<size_t>(children_count_tmp);
    for (size_t i = 0; i < children_count; ++i) {
      children.emplace_back();
      std::optional<base::span<const uint8_t>> span = iterator.ReadData();
      if (!span.has_value()) {
        return false;
      }
      if (!children.back().FromPickle(
              base::PickleIterator::WithData(span.value()))) {
        return false;
      }
    }
  }
  return true;
}
#endif

// BookmarkNodeData -----------------------------------------------------------

BookmarkNodeData::BookmarkNodeData() = default;

BookmarkNodeData::BookmarkNodeData(const BookmarkNodeData& other) = default;

BookmarkNodeData::BookmarkNodeData(const BookmarkNode* node) {
  elements.emplace_back(node);
}

BookmarkNodeData::BookmarkNodeData(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>& nodes) {
  ReadFromVector(nodes);
}

BookmarkNodeData::~BookmarkNodeData() {}

#if !BUILDFLAG(IS_APPLE)
// static
bool BookmarkNodeData::ClipboardContainsBookmarks() {
  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, {.notify_if_restricted = false});
  return ui::Clipboard::GetForCurrentThread()->IsFormatAvailable(
      ui::ClipboardFormatType::CustomPlatformType(kClipboardFormatString),
      ui::ClipboardBuffer::kCopyPaste, &data_dst);
}
#endif

bool BookmarkNodeData::ReadFromVector(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>& nodes) {
  Clear();

  if (nodes.empty()) {
    return false;
  }

  for (const auto& node : nodes) {
    elements.emplace_back(node);
  }

  CHECK(is_valid());
  return true;
}

bool BookmarkNodeData::ReadFromTuple(const GURL& url,
                                     const std::u16string& title) {
  Clear();

  if (!url.is_valid()) {
    return false;
  }

  Element element;
  element.title = title;
  element.url = url;
  element.is_url = true;

  elements.push_back(element);

  CHECK(is_valid());
  return true;
}

#if !BUILDFLAG(IS_APPLE)
void BookmarkNodeData::WriteToClipboard(bool is_off_the_record) {
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);

  if (is_off_the_record) {
    // Data is copied from an incognito window, so mark it as off the record.
    scw.MarkAsOffTheRecord();
  }

#if BUILDFLAG(IS_WIN)
  const std::u16string kEOL(u"\r\n");
#else
  const std::u16string kEOL = u"\n";
#endif

  // If there is only one element and it is a URL, write the URL to the
  // clipboard.
  if (has_single_url()) {
    const std::u16string& title = elements[0].title;
    const std::string url = elements[0].url.spec();

    scw.WriteBookmark(title, url);
    scw.WriteHyperlink(title, url);
    scw.WriteText(base::UTF8ToUTF16(url));
  } else {
    // We have either more than one URL, a folder, or a combination of URLs
    // and folders.
    std::u16string text;
    for (size_t i = 0; i < size(); i++) {
      text += i == 0 ? u"" : kEOL;
      if (!elements[i].is_url) {
        // Then it's a folder. Only copy the name of the folder.
        const std::u16string title = elements[i].title;
        text += title;
      } else {
        const std::u16string url = base::UTF8ToUTF16(elements[i].url.spec());
        text += url;
      }
    }
    scw.WriteText(text);
  }

  base::Pickle pickle;
  WriteToPickle(base::FilePath(), &pickle);
  scw.WritePickledData(pickle, ui::ClipboardFormatType::CustomPlatformType(
                                   kClipboardFormatString));
}

bool BookmarkNodeData::ReadFromClipboard(ui::ClipboardBuffer buffer) {
  DCHECK_EQ(buffer, ui::ClipboardBuffer::kCopyPaste);
  std::string data;
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  clipboard->ReadData(
      ui::ClipboardFormatType::CustomPlatformType(kClipboardFormatString),
      /* data_dst = */ nullptr, &data);

  if (!data.empty()) {
    if (ReadFromPickle(
            base::PickleIterator::WithData(base::as_byte_span(data)))) {
      CHECK(is_valid());
      return true;
    }
  }

  std::u16string title;
  std::string url;
  clipboard->ReadBookmark(/* data_dst = */ nullptr, &title, &url);
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
  if (base::FeatureList::IsEnabled(kEnableBookmarkNodeDataNewPickleFormat)) {
    // Backward-compatibility safeguard (as old versions cannot read the new
    // format).
    pickle->WriteUInt32(0);
    pickle->WriteUInt32(static_cast<uint32_t>(size()));
    for (const auto& element : elements) {
      pickle->WriteData(element.ToPickle());
    }
  } else {
    pickle->WriteUInt32(static_cast<uint32_t>(size()));
    for (const auto& element : elements) {
      element.WriteToLegacyPickle(pickle);
    }
  }
}

bool BookmarkNodeData::ReadFromPickle(base::PickleIterator data_iterator) {
  uint32_t element_count_tmp = 0;
  if (!profile_path_.ReadFromPickle(&data_iterator) ||
      !data_iterator.ReadUInt32(&element_count_tmp)) {
    return false;
  }

  // legacy format: profile_path, element_count, elements
  // newer format: profile_path, 0, element_count, elements
  bool legacy_format = (element_count_tmp != 0);
  if (!legacy_format) {
    if (!data_iterator.ReadUInt32(&element_count_tmp)) {
      return false;
    }
  }

  if (!base::IsValueInRangeForNumericType<size_t>(element_count_tmp)) {
    LOG(WARNING) << "element_count failed bounds check";
    return false;
  }
  const size_t element_count = base::checked_cast<size_t>(element_count_tmp);
  // Restrict vector preallocation to prevent OOM crashes on invalid or
  // malicious pickles.
  if (element_count > kMaxVectorPreallocateSize) {
    LOG(WARNING) << "element_count exceeds kMaxVectorPreallocateSize";
  }
  std::vector<Element> tmp_elements;
  tmp_elements.reserve(std::min(element_count, kMaxVectorPreallocateSize));
  if (legacy_format) {
    for (size_t i = 0; i < element_count; ++i) {
      tmp_elements.emplace_back();
      if (!tmp_elements.back().ReadFromLegacyPickle(&data_iterator)) {
        return false;
      }
    }
  } else {
    for (size_t i = 0; i < element_count; ++i) {
      std::optional<base::span<const uint8_t>> span = data_iterator.ReadData();
      if (!span.has_value()) {
        return false;
      }
      tmp_elements.emplace_back();
      if (!tmp_elements.back().FromPickle(
              base::PickleIterator::WithData(span.value()))) {
        return false;
      }
    }
  }
  elements.swap(tmp_elements);

  // Treat the data loading from pickle as a failure if elements is empty, which
  // forces the `ReadFromClipboard` fallback to continue reading data via
  // `ReadBookmark`. Otherwise, returning true while elements is empty will
  // trigger a CHECK. See http://crbug.com/472245783 for details.
  return is_valid();
}
#endif  // BUILDFLAG(IS_APPLE)

std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>
BookmarkNodeData::GetNodes(BookmarkModel* model,
                           const base::FilePath& profile_path) const {
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes;

  if (!IsFromProfilePath(profile_path)) {
    return nodes;
  }

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
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes =
      GetNodes(model, profile_path);
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
