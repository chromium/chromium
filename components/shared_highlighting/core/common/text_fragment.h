// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_TEXT_FRAGMENT_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_TEXT_FRAGMENT_H_

#include <string>

namespace shared_highlighting {

// Class representing a text fragment.
class TextFragment {
 public:
  explicit TextFragment(const std::string& text_start);
  TextFragment(const std::string& text_start,
               const std::string& text_end,
               const std::string& prefix,
               const std::string& suffix);
  TextFragment(const TextFragment& other);
  ~TextFragment();

  const std::string text_start() const { return text_start_; }
  const std::string text_end() const { return text_end_; }
  const std::string prefix() const { return prefix_; }
  const std::string suffix() const { return suffix_; }

  // Converts the current fragment to its URL parameter format:
  // text=[prefix-,]textStart[,textEnd][,-suffix]
  // Returns an empty string if |text_start| does not have a value.
  std::string ToString();

 private:
  std::string text_start_;
  std::string text_end_;
  std::string prefix_;
  std::string suffix_;
};

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_TEXT_FRAGMENT_H_
