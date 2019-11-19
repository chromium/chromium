// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/logging/log_buffer.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill {

namespace {

bool IsElement(const base::Value& value) {
  const std::string* type = value.FindStringKey("type");
  return type && *type == "element";
}

bool IsTextNode(const base::Value& value) {
  const std::string* type = value.FindStringKey("type");
  return type && *type == "text";
}

bool IsFragment(const base::Value& value) {
  const std::string* type = value.FindStringKey("type");
  return type && *type == "fragment";
}

void AppendChildToLastNode(std::vector<base::Value>* buffer,
                           base::Value&& new_child) {
  if (buffer->empty()) {
    buffer->push_back(std::move(new_child));
    return;
  }

  base::Value& parent = buffer->back();
  // Elements and Fragments can have children, but TextNodes cannot.
  DCHECK(!IsTextNode(parent));

  if (auto* children = parent.FindListKey("children")) {
    children->Append(std::move(new_child));
    return;
  }

  base::Value::ListStorage list;
  list.emplace_back(std::move(new_child));
  parent.SetKey("children", base::Value(std::move(list)));
}

// This is an optimization to reduce the number of text nodes in the DOM.
// Sequences of appended StringPieces are coalesced into one. If many strings
// are appended, this has quadratic runtime. But the number of strings
// and the lengths of strings should be relatively small and we reduce the
// memory consumption of the DOM, which may grow rather large.
//
// If the last child of the element in buffer is a text node, append |text| to
// it and return true (successful coalescing). Otherwise return false.
bool TryCoalesceString(std::vector<base::Value>* buffer,
                       base::StringPiece text) {
  if (buffer->empty())
    return false;
  base::Value& parent = buffer->back();
  auto* children = parent.FindListKey("children");
  if (!children)
    return false;
  DCHECK(!children->GetList().empty());
  auto& last_child = children->GetList().back();
  if (!IsTextNode(last_child))
    return false;
  std::string* old_text = last_child.FindStringKey("value");
  old_text->append(text.data(), text.size());
  return true;
}

base::Value CreateEmptyFragment() {
  base::Value::DictStorage storage;
  storage.try_emplace("type", std::make_unique<base::Value>("fragment"));
  return base::Value(storage);
}

}  // namespace

LogBuffer::LogBuffer() {
  buffer_.push_back(CreateEmptyFragment());
}

LogBuffer::LogBuffer(LogBuffer&& other) noexcept = default;
LogBuffer::~LogBuffer() = default;

base::Value LogBuffer::RetrieveResult() {
  // The buffer should always start with a fragment.
  DCHECK(buffer_.size() >= 1);

  // Close not-yet-closed tags.
  while (buffer_.size() > 1)
    *this << CTag{};

  auto* children = buffer_[0].FindListKey("children");
  if (!children || children->GetList().empty())
    return base::Value();

  // If the fragment has a single child, remove it from |children| and return
  // that directly.
  if (children->GetList().size() == 1) {
    return std::move(children->TakeList().back());
  }

  return std::exchange(buffer_.back(), CreateEmptyFragment());
}

LogBuffer& operator<<(LogBuffer& buf, Tag&& tag) {
  if (!buf.active())
    return buf;

  base::Value::DictStorage storage;
  storage.try_emplace("type", std::make_unique<base::Value>("element"));
  storage.try_emplace("value",
                      std::make_unique<base::Value>(std::move(tag.name)));
  buf.buffer_.emplace_back(std::move(storage));
  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, CTag&& tag) {
  if (!buf.active())
    return buf;
  // Don't close the fragment. It stays and gets returned in the end.
  if (buf.buffer_.size() <= 1)
    return buf;

  base::Value node_to_add = std::move(buf.buffer_.back());
  buf.buffer_.pop_back();

  AppendChildToLastNode(&buf.buffer_, std::move(node_to_add));
  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, Attrib&& attrib) {
  if (!buf.active())
    return buf;

  base::Value& node = buf.buffer_.back();
  DCHECK(IsElement(node));

  if (auto* attributes = node.FindDictKey("attributes")) {
    attributes->SetKey(std::move(attrib.name),
                       base::Value(std::move(attrib.value)));
  } else {
    base::Value::DictStorage dict;
    dict.try_emplace(std::move(attrib.name),
                     std::make_unique<base::Value>(std::move(attrib.value)));
    node.SetKey("attributes", base::Value(std::move(dict)));
  }

  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, Br&& tag) {
  if (!buf.active())
    return buf;
  return buf << Tag{"br"} << CTag{};
}

LogBuffer& operator<<(LogBuffer& buf, base::StringPiece text) {
  if (!buf.active())
    return buf;

  if (text.empty())
    return buf;

  if (TryCoalesceString(&buf.buffer_, text))
    return buf;

  base::Value::DictStorage storage;
  storage.try_emplace("type", std::make_unique<base::Value>("text"));
  // This text is not HTML escaped because the rest of the frame work takes care
  // of that and it must not be escaped twice.
  storage.try_emplace("value", std::make_unique<base::Value>(text));
  base::Value node_to_add(std::move(storage));
  AppendChildToLastNode(&buf.buffer_, std::move(node_to_add));
  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, base::StringPiece16 text) {
  return buf << base::UTF16ToUTF8(text);
}

LogBuffer& operator<<(LogBuffer& buf, LogBuffer&& buffer) {
  if (!buf.active())
    return buf;

  base::Value node_to_add(buffer.RetrieveResult());
  if (node_to_add.is_none())
    return buf;

  if (IsFragment(node_to_add)) {
    auto* children = node_to_add.FindListKey("children");
    if (!children)
      return buf;
    for (auto& child : children->GetList())
      AppendChildToLastNode(&buf.buffer_, std::exchange(child, base::Value()));
    return buf;
  }
  AppendChildToLastNode(&buf.buffer_, std::move(node_to_add));
  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, const GURL& url) {
  if (!buf.active())
    return buf;
  if (!url.is_valid())
    return buf << "Invalid URL";
  return buf << url.GetOrigin().spec();
}

LogTableRowBuffer::LogTableRowBuffer(LogBuffer* parent) : parent_(parent) {
  *parent_ << Tag{"tr"};
}

LogTableRowBuffer::LogTableRowBuffer(LogTableRowBuffer&& buffer) noexcept
    : parent_(buffer.parent_) {
  // Prevent double closing of the <tr> tag.
  buffer.parent_ = nullptr;
}

LogTableRowBuffer::~LogTableRowBuffer() {
  if (parent_)
    *parent_ << CTag{};
}

LogTableRowBuffer operator<<(LogBuffer& buf, Tr&& tr) {
  return LogTableRowBuffer(&buf);
}

LogTableRowBuffer&& operator<<(LogTableRowBuffer&& buf, Attrib&& attrib) {
  *buf.parent_ << std::move(attrib);
  return std::move(buf);
}

namespace {
// Highlights the first |needle| in |haystack| by wrapping it in <b> tags.
template <typename STRING_TYPE>
LogBuffer HighlightValueInternal(base::BasicStringPiece<STRING_TYPE> haystack,
                                 base::BasicStringPiece<STRING_TYPE> needle) {
  using StringPieceT = base::BasicStringPiece<STRING_TYPE>;
  LogBuffer buffer;
  size_t pos = haystack.find(needle);
  if (pos == StringPieceT::npos || needle.empty()) {
    buffer << haystack;
    return buffer;
  }
  buffer << haystack.substr(0, pos);
  buffer << Tag{"b"} << needle << CTag{"b"};
  buffer << haystack.substr(pos + needle.size());
  return buffer;
}
}  // namespace

LogBuffer HighlightValue(base::StringPiece haystack, base::StringPiece needle) {
  return HighlightValueInternal(haystack, needle);
}

LogBuffer HighlightValue(base::StringPiece16 haystack,
                         base::StringPiece16 needle) {
  return HighlightValueInternal(haystack, needle);
}

}  // namespace autofill
