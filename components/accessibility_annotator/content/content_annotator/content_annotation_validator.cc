// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/content/content_annotator/content_annotation_validator.h"

#include <algorithm>

#include "base/debug/dump_without_crashing.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"

namespace accessibility_annotator {

namespace {

bool IsInvalidChar(char c) {
  if (c == '<' || c == '>') {
    return true;
  }
  if (base::IsAsciiControl(c) && !base::IsAsciiWhitespace(c)) {
    return true;
  }
  return false;
}

}  // namespace

// static
std::unique_ptr<ContentAnnotationValidator>
ContentAnnotationValidator::Create() {
  std::string schema_json =
      kContentAnnotatorExtractedDataValidationSchema.Get();

  if (schema_json.empty()) {
    return std::make_unique<ContentAnnotationValidator>(base::DictValue());
  }

  std::optional<base::DictValue> parsed_json =
      base::JSONReader::ReadDict(schema_json, base::JSON_PARSE_RFC);

  if (!parsed_json.has_value()) {
    // Invalid JSON schema indicates a malformed flag value.
    base::debug::DumpWithoutCrashing();
    return nullptr;
  }

  return std::make_unique<ContentAnnotationValidator>(std::move(*parsed_json));
}

ContentAnnotationValidator::ContentAnnotationValidator(base::DictValue schema)
    : schema_(std::move(schema)) {}

ContentAnnotationValidator::~ContentAnnotationValidator() = default;

std::optional<base::DictValue> ContentAnnotationValidator::Validate(
    std::string extracted_data) const {
  // Regardless of schema, always reject data with invalid characters or JSON.
  if (std::ranges::any_of(extracted_data, IsInvalidChar)) {
    // For safety, reject data with HTML tags or control characters to create a
    // trusted data set.
    return std::nullopt;
  }

  std::optional<base::Value> parsed_data =
      base::JSONReader::Read(extracted_data, base::JSON_PARSE_RFC);
  if (!parsed_data || !parsed_data->is_dict()) {
    return std::nullopt;
  }

  // TODO(crbug.com/492271405): Implement top-level and extracted data
  // validation when validator is enabled.
  return std::move(*parsed_data).TakeDict();
}

}  // namespace accessibility_annotator
