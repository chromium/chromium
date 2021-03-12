// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_LOGGING_LOG_BUFFER_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_LOGGING_LOG_BUFFER_H_

#include <string>
#include <type_traits>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "url/gurl.h"

// The desired pattern to generate log messages is to pass a scope, a log
// message and then parameters.
//
// LogBuffer() << LoggingScope::kSomeScope << LogMessage::kSomeLogMessage
//     << Br{} << more << Br{} << parameters;
//
// Extra parameters can be:
//
// - numeric:
//   LogBuffer() << ... << 42;
//
// - inline strings:
//   LogBuffer() << ... << "foobar";
//
// - tags:
//   LogBuffer() << Tag{"div"} << ... << CTag{};
//   Note that tags need to be closed (even for <br> - use Br{} as it takes care
//   of generating an opening and closing tag). You may optionally specify what
//   tag is closed: CTag{"div"}.
//   Tags can get attributes via Attrib:
//   LogBuffer() << Tag{"div"} << Attrib{"class", "foobar"}... << CTag{};
//
// - objects that can have an overloaded operator:
//   LogBuffer& operator<<(LogBuffer& buf,
//                                       const SampleObject& value) {...}
//   LogBuffer() << ... << my_sample_object;
//
// - complex messages that require for loops:
//   LogBuffer buffer;
//   for (...) { buffer << something; }
//   LogBuffer() << std::move(buffer);

namespace autofill {

// Tag of HTML Element (e.g. <div> would be represented by Tag{"div"}). Note
// that every element needs to be closed with a CTag{}.
struct Tag {
  std::string name;
};

// The closing tag of an HTML Element.
struct CTag {
  CTag() = default;
  // |opt_name| is not used, and only exists for readability.
  explicit CTag(base::StringPiece opt_name) {}
};

// Attribute of an HTML Tag (e.g. class="foo" would be represented by
// Attrib{"class", "foo"}.
struct Attrib {
  std::string name;
  std::string value;
};

// An <br> HTML tag, note that this does not need to be closed.
struct Br {};

// A table row tag. This is syntactic sugar for logging data into a table.
// See LogTableRowBuffer below.
struct Tr {};

// A buffer into which you can stream values. See the top of this header file
// for samples.
class LogBuffer {
 public:
  LogBuffer();
  LogBuffer(LogBuffer&& other) noexcept;
  ~LogBuffer();

  // Returns the contents of the buffer and empties it.
  base::Value RetrieveResult();

  // Returns whether an active WebUI is listening. If false, the buffer may
  // not do any logging.
  bool active() const { return active_; }
  void set_active(bool active) { active_ = active; }

 private:
  friend LogBuffer& operator<<(LogBuffer& buf, Tag&& tag);
  friend LogBuffer& operator<<(LogBuffer& buf, CTag&& tag);
  friend LogBuffer& operator<<(LogBuffer& buf, Attrib&& attrib);
  friend LogBuffer& operator<<(LogBuffer& buf, base::StringPiece text);
  friend LogBuffer& operator<<(LogBuffer& buf, LogBuffer&& buffer);

  // The stack of values being constructed. Each item is a dictionary with the
  // following attributes:
  // - type: 'element' | 'fragment' | 'text'
  // - value: name of tag | text content
  // - children (opt): list of child nodes
  // - attributes (opt): dictionary of name/value pairs
  // The |buffer_| serves as a stack where the last element is being
  // constructed. Once it is read (i.e. closed via a CTag), it is popped from
  // the stack and attached as a child of the previously second last element.
  // Only the first element of buffer_ is a 'fragment' and it is never closed.
  std::vector<base::Value> buffer_;

  bool active_ = true;

  DISALLOW_COPY_AND_ASSIGN(LogBuffer);
};

// Enable streaming numbers of all types.
template <typename T,
          typename = std::enable_if_t<std::is_arithmetic<T>::value, T>>
LogBuffer& operator<<(LogBuffer& buf, T number) {
  return buf << base::NumberToString(number);
}

LogBuffer& operator<<(LogBuffer& buf, Tag&& tag);

LogBuffer& operator<<(LogBuffer& buf, CTag&& tag);

LogBuffer& operator<<(LogBuffer& buf, Attrib&& attrib);

LogBuffer& operator<<(LogBuffer& buf, Br&& tag);

LogBuffer& operator<<(LogBuffer& buf, base::StringPiece text);

LogBuffer& operator<<(LogBuffer& buf, base::StringPiece16 text);

// Sometimes you may want to fill a buffer that you then stream as a whole
// to LOG_AF_INTERNALS, which commits the data to chrome://autofill-internals:
//
//   LogBuffer buffer;
//   for (FormStructure* form : forms)
//     buffer << *form;
//   LogBuffer() << LoggingScope::kParsing << std::move(buffer);
//
// It would not be possible to report all |forms| into a single log entry
// without this.
LogBuffer& operator<<(LogBuffer& buf, LogBuffer&& buffer);

// Streams only the security origin of the URL. This is done for privacy
// reasons.
LogBuffer& operator<<(LogBuffer& buf, const GURL& url);

template <typename T>
LogBuffer& operator<<(LogBuffer& buf, const std::vector<T>& values) {
  buf << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i)
      buf << ", ";
    buf << values.at(i);
  }
  buf << "]";
  return buf;
}

// This is syntactic sugar for creating table rows in a LogBuffer. Each
// value streamed into this LogTableRowBuffer is wrapped by a <td> element.
// The entire row is wrapped by a <tr>.
//
// Here is an example:
//   LogBuffer buf;
//   buf << Tr{} << Attrib{"style", "color: red"} << "Foo" << "Bar";
// This creates:
//   <tr style="color: red"><td>Foo</td><td>Bar</td></tr>.
class LogTableRowBuffer {
 public:
  explicit LogTableRowBuffer(LogBuffer* parent);
  LogTableRowBuffer(LogTableRowBuffer&& buffer) noexcept;
  ~LogTableRowBuffer();

 private:
  template <typename T>
  friend LogTableRowBuffer&& operator<<(LogTableRowBuffer&& buf, T&& value);
  friend LogTableRowBuffer&& operator<<(LogTableRowBuffer&& buf,
                                        Attrib&& attrib);

  LogBuffer* parent_ = nullptr;
};

LogTableRowBuffer operator<<(LogBuffer& buf, Tr&& tr);

template <typename T>
LogTableRowBuffer&& operator<<(LogTableRowBuffer&& buf, T&& value) {
  *buf.parent_ << Tag{"td"} << std::forward<T>(value) << CTag{};
  return std::move(buf);
}

LogTableRowBuffer&& operator<<(LogTableRowBuffer&& buf, Attrib&& attrib);

// Highlights the first |needle| in |haystack| by wrapping it in <b> tags.
LogBuffer HighlightValue(base::StringPiece haystack, base::StringPiece needle);
LogBuffer HighlightValue(base::StringPiece16 haystack,
                         base::StringPiece16 needle);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_LOGGING_LOG_BUFFER_H_
