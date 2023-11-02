// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_PATTERN_PARSER_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_PATTERN_PARSER_H_

#include <string>

#include "base/strings/string_piece_forward.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace content_settings {

class PatternParser {
 public:
  PatternParser() = delete;
  PatternParser(const PatternParser&) = delete;
  PatternParser& operator=(const PatternParser&) = delete;

  static void Parse(base::StringPiece pattern_spec,
                    ContentSettingsPattern::BuilderInterface* builder);

  static std::string ToString(
      const ContentSettingsPattern::PatternParts& parts);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_CONTENT_SETTINGS_PATTERN_PARSER_H_
