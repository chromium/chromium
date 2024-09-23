// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ini_parser.h"

#include <stddef.h>

#include <string_view>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_tokenizer.h"
#include "base/values.h"

INIParser::INIParser() : used_(false) {}

INIParser::~INIParser() {}

void INIParser::Parse(const std::string& content) {
  DCHECK(!used_);
  used_ = true;
  base::StringTokenizer tokenizer(content, "\r\n");

  std::string_view current_section;
  while (tokenizer.GetNext()) {
    std::string_view line = tokenizer.token_piece();
    if (line.empty()) {
      // Skips the empty line.
      continue;
    }
    if (line[0] == '#' || line[0] == ';') {
      // This line is a comment.
      continue;
    }
    if (line[0] == '[') {
      // It is a section header.
      current_section = line.substr(1);
      size_t end = current_section.rfind(']');
      if (end != std::string::npos)
        current_section = current_section.substr(0, end);
    } else {
      std::string_view key, value;
      size_t equal = line.find('=');
      if (equal != std::string::npos) {
        key = line.substr(0, equal);
        value = line.substr(equal + 1);
        HandleTriplet(current_section, key, value);
      }
    }
  }
}

DictionaryValueINIParser::DictionaryValueINIParser() {}

DictionaryValueINIParser::~DictionaryValueINIParser() {}

void DictionaryValueINIParser::HandleTriplet(std::string_view section,
                                             std::string_view key,
                                             std::string_view value) {
  // Checks whether the section and key contain a '.' character.
  // Those sections and keys break `base::Value::Dict`'s path format when not
  // using the *WithoutPathExpansion methods.
  if (section.find('.') == std::string::npos &&
      key.find('.') == std::string::npos)
    root_.SetByDottedPath(base::StrCat({section, ".", key}), value);
}
