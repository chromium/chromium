// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/logging/log_buffer.h"

#include <string>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill {

namespace {

bool IsElement(const base::Value::Dict& value) {
  const std::string* type = value.FindString("type");
  return type && *type == "element";
}

bool IsTextNode(const base::Value::Dict& value) {
  const std::string* type = value.FindString("type");
  return type && *type == "text";
}

bool IsFragment(const base::Value::Dict& value) {
  const std::string* type = value.FindString("type");
  return type && *type == "fragment";
}

void AppendChildToLastNode(std::vector<base::Value::Dict>* buffer,
                           base::Value::Dict&& new_child) {
  if (buffer->empty()) {
    buffer->push_back(std::move(new_child));
    return;
  }

  base::Value::Dict& parent = buffer->back();
  // Elements and Fragments can have children, but TextNodes cannot.
  DCHECK(!IsTextNode(parent));

  if (auto* children = parent.FindList("children")) {
    children->Append(std::move(new_child));
    return;
  }

  base::Value::List list;
  list.Append(std::move(new_child));
  parent.Set("children", std::move(list));
}

// This is an optimization to reduce the number of text nodes in the DOM.
// Sequences of appended string_views are coalesced into one. If many strings
// are appended, this has quadratic runtime. But the number of strings
// and the lengths of strings should be relatively small and we reduce the
// memory consumption of the DOM, which may grow rather large.
//
// If the last child of the element in buffer is a text node, append |text| to
// it and return true (successful coalescing). Otherwise return false.
bool TryCoalesceString(std::vector<base::Value::Dict>* buffer,
                       std::string_view text) {
  if (buffer->empty())
    return false;
  base::Value::Dict& parent = buffer->back();
  auto* children = parent.FindList("children");
  if (!children)
    return false;
  DCHECK(!children->empty());
  auto& last_child = children->back().GetDict();
  if (!IsTextNode(last_child))
    return false;
  std::string* old_text = last_child.FindString("value");
  old_text->append(text);
  return true;
}

base::Value::Dict CreateEmptyFragment() {
  base::Value::Dict dict;
  dict.Set("type", "fragment");
  return dict;
}

}  // namespace

LogBuffer::LogBuffer(IsActive active) : active_(*active) {
  if (active_)
    buffer_.push_back(CreateEmptyFragment());
}

LogBuffer::LogBuffer(LogBuffer&& other) noexcept = default;
LogBuffer& LogBuffer::operator=(LogBuffer&& other) = default;
LogBuffer::~LogBuffer() = default;

std::optional<base::Value::Dict> LogBuffer::RetrieveResult() {
  // The buffer should always start with a fragment.
  DCHECK(buffer_.size() >= 1);

  // Close not-yet-closed tags.
  while (buffer_.size() > 1)
    *this << CTag{};

  auto* children = buffer_[0].FindList("children");
  if (!children || children->empty())
    return std::nullopt;

  // If the fragment has a single child, remove it from |children| and return
  // that directly.
  if (children->size() == 1) {
    return std::optional<base::Value::Dict>(
        std::move((*children).back().GetDict()));
  }

  return std::exchange(buffer_.back(), CreateEmptyFragment());
}

LogBuffer& operator<<(LogBuffer& buf, Tag&& tag) {
  if (!buf.active())
    return buf;

  base::Value::Dict dict;
  dict.Set("type", "element");
  dict.Set("value", std::move(tag.name));
  buf.buffer_.emplace_back(std::move(dict));
  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, CTag&& tag) {
  if (!buf.active())
    return buf;
  // Don't close the fragment. It stays and gets returned in the end.
  if (buf.buffer_.size() <= 1)
    return buf;

  base::Value::Dict node_to_add = std::move(buf.buffer_.back());
  buf.buffer_.pop_back();

  AppendChildToLastNode(&buf.buffer_, std::move(node_to_add));
  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, Attrib&& attrib) {
  if (!buf.active())
    return buf;

  base::Value::Dict& node = buf.buffer_.back();
  DCHECK(IsElement(node));

  if (auto* attributes = node.FindDict("attributes")) {
    attributes->Set(attrib.name, std::move(attrib.value));
  } else {
    base::Value::Dict dict;
    dict.Set(attrib.name, std::move(attrib.value));
    node.Set("attributes", std::move(dict));
  }

  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, Br&& tag) {
  if (!buf.active())
    return buf;
  return buf << Tag{"br"} << CTag{};
}

LogBuffer& operator<<(LogBuffer& buf, std::string_view text) {
  if (!buf.active())
    return buf;

  if (text.empty())
    return buf;

  if (TryCoalesceString(&buf.buffer_, text))
    return buf;

  base::Value::Dict node_to_add;
  node_to_add.Set("type", "text");
  // This text is not HTML escaped because the rest of the frame work takes care
  // of that and it must not be escaped twice.
  node_to_add.Set("value", text);
  AppendChildToLastNode(&buf.buffer_, std::move(node_to_add));
  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, std::u16string_view text) {
  return buf << base::UTF16ToUTF8(text);
}

LogBuffer& operator<<(LogBuffer& buf, LogBuffer&& buffer) {
  if (!buf.active())
    return buf;

  std::optional<base::Value::Dict> node_to_add = buffer.RetrieveResult();
  if (!node_to_add)
    return buf;

  if (IsFragment(*node_to_add)) {
    auto* children = node_to_add->FindList("children");
    if (!children)
      return buf;
    for (auto& child : *children) {
      AppendChildToLastNode(
          &buf.buffer_, std::exchange(child.GetDict(), base::Value::Dict()));
    }
    return buf;
  }
  AppendChildToLastNode(&buf.buffer_, std::move(*node_to_add));
  return buf;
}

LogBuffer& operator<<(LogBuffer& buf, const GURL& url) {
  if (!buf.active())
    return buf;
  if (!url.is_valid())
    return buf << "Invalid URL";
  return buf << url.DeprecatedGetOriginAsURL().spec();
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

LogTableRowBuffer&& operator<<(LogTableRowBuffer&& buf,
                               SetParentTagContainsPII&& attrib) {
  *buf.parent_ << Attrib{"data-pii", "true"};
  return std::move(buf);
}

namespace {
// Highlights the first |needle| in |haystack| by wrapping it in <b> tags.
template <typename T, typename CharT = typename T::value_type>
LogBuffer HighlightValueInternal(T haystack, T needle) {
  using StringViewT = std::basic_string_view<CharT>;
  LogBuffer buffer(LogBuffer::IsActive(true));
  size_t pos = haystack.find(needle);
  if (pos == StringViewT::npos || needle.empty()) {
    buffer << haystack;
    return buffer;
  }
  buffer << haystack.substr(0, pos);
  buffer << Tag{"b"} << needle << CTag{"b"};
  buffer << haystack.substr(pos + needle.size());
  return buffer;
}
}  // namespace

LogBuffer HighlightValue(std::string_view haystack, std::string_view needle) {
  return HighlightValueInternal(haystack, needle);
}

LogBuffer HighlightValue(std::u16string_view haystack,
                         std::u16string_view needle) {
  return HighlightValueInternal(haystack, needle);
}

}  // namespace autofill
