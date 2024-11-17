// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_ANSWER_H_
#define COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_ANSWER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "third_party/omnibox_proto/answer_data.pb.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "url/gurl.h"

namespace omnibox::answer_data_parser {
// These values are named and numbered to match a specification at go/ais_api.
// The values are only used for answer results.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.omnibox
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: AnswerTextType
enum TextType {
  // Deprecated: ANSWER = 1,
  // Deprecated: HEADLINE = 2,
  TOP_ALIGNED = 3,
  // Deprecated: DESCRIPTION = 4,
  DESCRIPTION_NEGATIVE = 5,
  DESCRIPTION_POSITIVE = 6,
  // Deprecated: MORE_INFO = 7,
  SUGGESTION = 8,
  // Deprecated: SUGGESTION_POSITIVE = 9,
  // Deprecated: SUGGESTION_NEGATIVE = 10,
  // Deprecated: SUGGESTION_LINK = 11,
  // Deprecated: STATUS = 12,
  PERSONALIZED_SUGGESTION = 13,
  // Deprecated: IMMERSIVE_DESCRIPTION_TEXT = 14,
  // Deprecated: DATE_TEXT = 15,
  // Deprecated: PREVIEW_TEXT = 16,
  ANSWER_TEXT_MEDIUM = 17,
  ANSWER_TEXT_LARGE = 18,
  SUGGESTION_SECONDARY_TEXT_SMALL = 19,
  SUGGESTION_SECONDARY_TEXT_MEDIUM = 20,
};

GURL GetFormattedURL(const std::string* url_string);

bool ParseJsonToAnswerData(const base::Value::Dict& answer_json,
                           omnibox::RichAnswerTemplate* answer_template);

// Logs which answer type was used (if any) at the time a user used the
// omnibox to go somewhere.
void LogAnswerUsed(omnibox::AnswerType answer_type);
}  // namespace omnibox::answer_data_parser

#endif  // COMPONENTS_OMNIBOX_BROWSER_SUGGESTION_ANSWER_H_
