// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/text_fragment.h"

#include <sstream>

#include "base/strings/string_util.h"
#include "components/shared_highlighting/core/common/text_fragments_constants.h"
#include "net/base/escape.h"

namespace {

std::string Escape(std::string str) {
  std::string escaped = net::EscapeQueryParamValue(str, /*usePlus=*/false);

  // Hyphens must also be escaped since they are used to indicate prefix/suffix
  // components.
  std::string final_string;
  base::ReplaceChars(escaped, "-", "%2D", &final_string);
  return final_string;
}

}  // namespace

namespace shared_highlighting {

TextFragment::TextFragment(const std::string& text_start)
    : TextFragment(text_start, std::string(), std::string(), std::string()) {}

TextFragment::TextFragment(const std::string& text_start,
                           const std::string& text_end,
                           const std::string& prefix,
                           const std::string& suffix)
    : text_start_(text_start),
      text_end_(text_end),
      prefix_(prefix),
      suffix_(suffix) {}

TextFragment::TextFragment(const TextFragment& other)
    : TextFragment(other.text_start(),
                   other.text_end(),
                   other.prefix(),
                   other.suffix()) {}

TextFragment::~TextFragment() = default;

std::string TextFragment::ToString() {
  if (text_start_.empty()) {
    return std::string();
  }
  std::stringstream ss;
  ss << kFragmentParameterName;

  if (!prefix_.empty()) {
    ss << Escape(prefix_) << "-,";
  }

  ss << Escape(text_start_);

  if (!text_end_.empty()) {
    ss << "," << Escape(text_end_);
  }

  if (!suffix_.empty()) {
    ss << ",-" << Escape(suffix_);
  }

  return ss.str();
}

}  // namespace shared_highlighting
