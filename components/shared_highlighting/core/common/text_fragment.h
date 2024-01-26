// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_TEXT_FRAGMENT_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_TEXT_FRAGMENT_H_

#include <optional>
#include <string>

#include "base/values.h"

namespace shared_highlighting {

// Class representing a text fragment.
class TextFragment {
 public:
  // Constructors for TextFragment instances. Special characters in the string
  // parameters must not be escaped.
  explicit TextFragment(const std::string& text_start);
  TextFragment(const std::string& text_start,
               const std::string& text_end,
               const std::string& prefix,
               const std::string& suffix);
  TextFragment(const TextFragment& other);
  ~TextFragment();

  // Returns a TextFragment instance created from a |fragment_string| whose
  // special characters have been escaped. The given string is expected to have
  // the traditional text fragment format:
  // [prefix-,]textStart[,textEnd][,-suffix]
  // Returns |std::nullopt| if parsing failed.
  static std::optional<TextFragment> FromEscapedString(
      std::string escaped_string);

  // Returns a TextFragment instance created from a dictionary |value|
  // containing the right set of values. The values stored in |value| must be
  // already unescaped.
  // Returns |std::nullopt| if parsing failed.
  static std::optional<TextFragment> FromValue(const base::Value* value);

  const std::string text_start() const { return text_start_; }
  const std::string text_end() const { return text_end_; }
  const std::string prefix() const { return prefix_; }
  const std::string suffix() const { return suffix_; }

  // Converts the current fragment to its escaped URL parameter format:
  // text=[prefix-,]textStart[,textEnd][,-suffix]
  // Returns an empty string if |text_start| does not have a value.
  std::string ToEscapedString() const;

  // Converts the current fragment to a dictionary Value.
  base::Value ToValue() const;

 private:
  // Values of a fragment, stored unescaped.
  std::string text_start_;
  std::string text_end_;
  std::string prefix_;
  std::string suffix_;
};

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_TEXT_FRAGMENT_H_
