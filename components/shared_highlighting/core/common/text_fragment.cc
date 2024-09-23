// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/text_fragment.h"

#include <sstream>

#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/shared_highlighting/core/common/fragment_directives_constants.h"

namespace {

// Escapes any special character such that the fragment can be added to a URL.
std::string Escape(const std::string& str) {
  std::string escaped = base::EscapeQueryParamValue(str, /*usePlus=*/false);

  // Hyphens must also be escaped since they are used to indicate prefix/suffix
  // components.
  std::string final_string;
  base::ReplaceChars(escaped, "-", "%2D", &final_string);
  return final_string;
}

// Unescapes any special character from a fragment which may be coming from a
// URL. Returns nullopt if the fragment can't be safely escaped (e.g., contains
// non-UTF8 characters).
std::optional<std::string> Unescape(const std::string& str) {
  std::string unescaped = base::UnescapeBinaryURLComponent(str);
  if (!base::IsStringUTF8(unescaped)) {
    return std::nullopt;
  }
  return unescaped;
}

bool HasValue(const std::string* str) {
  return str && !str->empty();
}

const std::string ValueOrDefault(const std::string* str) {
  return HasValue(str) ? *str : "";
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

// static
std::optional<TextFragment> TextFragment::FromEscapedString(
    std::string escaped_string) {
  // Text fragments have the format: [prefix-,]textStart[,textEnd][,-suffix]
  // That is, textStart is the only required param, all params are separated by
  // commas, and prefix/suffix have a trailing/leading hyphen.
  // Any commas, ampersands, or hyphens inside of these values must be
  // URL-encoded.

  // First, try to extract the optional prefix and suffix params. These have a
  // '-' as their last or first character, respectively, which should not be
  // carried over to the final dict.
  std::string prefix = "";
  size_t prefix_delimiter_pos = escaped_string.find("-,");
  if (prefix_delimiter_pos != std::string::npos) {
    prefix = escaped_string.substr(0, prefix_delimiter_pos);
    escaped_string.erase(0, prefix_delimiter_pos + 2);
  }

  std::string suffix = "";
  size_t suffix_delimiter_pos = escaped_string.rfind(",-");
  if (suffix_delimiter_pos != std::string::npos) {
    suffix = escaped_string.substr(suffix_delimiter_pos + 2);
    escaped_string.erase(suffix_delimiter_pos);
  }

  std::vector<std::string> pieces = base::SplitString(
      escaped_string, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (pieces.size() > 2 || pieces.empty() || pieces[0].empty()) {
    // Malformed if no piece is left for the textStart
    return std::nullopt;
  }

  std::string text_start = pieces[0];
  std::string text_end = pieces.size() == 2 ? pieces[1] : "";

  if (prefix.find_first_of("&-,") != std::string::npos ||
      text_start.find_first_of("&-,") != std::string::npos ||
      text_end.find_first_of("&-,") != std::string::npos ||
      suffix.find_first_of("&-,") != std::string::npos) {
    // Malformed if any of the pieces contain characters that are supposed to be
    // URL-encoded.
    return std::nullopt;
  }

  std::optional<std::string> unescaped_text_start = Unescape(text_start),
                             unescaped_text_end = Unescape(text_end),
                             unescaped_prefix = Unescape(prefix),
                             unescaped_suffix = Unescape(suffix);

  if (!unescaped_text_start || !unescaped_text_end || !unescaped_prefix ||
      !unescaped_suffix) {
    return std::nullopt;
  }

  return TextFragment(*unescaped_text_start, *unescaped_text_end,
                      *unescaped_prefix, *unescaped_suffix);
}

// static
std::optional<TextFragment> TextFragment::FromValue(const base::Value* value) {
  if (!value || !value->is_dict()) {
    return std::nullopt;
  }

  const base::Value::Dict& dict = value->GetDict();
  const std::string* text_start = dict.FindString(kFragmentTextStartKey);
  const std::string* text_end = dict.FindString(kFragmentTextEndKey);
  const std::string* prefix = dict.FindString(kFragmentPrefixKey);
  const std::string* suffix = dict.FindString(kFragmentSuffixKey);

  if (!HasValue(text_start)) {
    // Text Start is the only required parameter.
    return std::nullopt;
  }

  return TextFragment(*text_start, ValueOrDefault(text_end),
                      ValueOrDefault(prefix), ValueOrDefault(suffix));
}

std::string TextFragment::ToEscapedString() const {
  if (text_start_.empty()) {
    return std::string();
  }
  std::stringstream ss;
  ss << kTextDirectiveParameterName;

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

base::Value TextFragment::ToValue() const {
  base::Value::Dict dict;

  if (prefix_.size())
    dict.Set(kFragmentPrefixKey, prefix_);

  dict.Set(kFragmentTextStartKey, text_start_);

  if (text_end_.size())
    dict.Set(kFragmentTextEndKey, text_end_);

  if (suffix_.size())
    dict.Set(kFragmentSuffixKey, suffix_);

  return base::Value(std::move(dict));
}

}  // namespace shared_highlighting
