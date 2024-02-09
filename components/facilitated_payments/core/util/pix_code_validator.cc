// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/util/pix_code_validator.h"

#include "third_party/re2/src/re2/re2.h"

namespace payments::facilitated {
namespace {

constexpr char kSectionIdAndSizePattern[] = "(\\d{2})(\\d{2})";
constexpr char kPayloadFormatIndicatorFirstSectionId[] = "00";
constexpr char kMerchantAccountInformationSectionId[] = "26";
constexpr char kAdditionalDataFieldTemplateSectionId[] = "62";
constexpr char kCrc16LastSectionId[] = "63";
constexpr char kPixCodeIndicator[] = "0014br.gov.bcb.pix";

struct SectionInfo {
  std::string_view section_id;
  std::string_view section_value;
};

// Returns true if `code` starts with a valid PIX `code` section.
// A valid PIX code section comprises:
// 1) Two digits of section ID.
// 2) Two digits of section size |N|.
// 3) Section value of length |N|.
bool ParseNextSection(std::string_view* code, SectionInfo* section_info) {
  static re2::LazyRE2 re = {kSectionIdAndSizePattern};
  size_t section_size = 0;
  if (!re2::RE2::Consume(code, *re, &section_info->section_id, &section_size)) {
    return false;
  }

  if (section_size > code->length()) {
    return false;
  }

  section_info->section_value = code->substr(0, section_size);
  code->remove_prefix(section_size);

  return true;
}

// Returns true if the `input` string consists of valid PIX code sections.
bool ContainsValidSections(std::string_view input) {
  if (input.empty()) {
    return false;
  }

  SectionInfo section_info;
  while (!input.empty()) {
    if (!ParseNextSection(&input, &section_info)) {
      return false;
    }
  }

  return true;
}

}  // namespace

bool IsValidPixCode(std::string_view code) {
  if (code.empty()) {
    return false;
  }

  SectionInfo section_info;
  bool contains_pix_code_indicator = false;

  if (!ParseNextSection(&code, &section_info) ||
      section_info.section_id != kPayloadFormatIndicatorFirstSectionId) {
    return false;
  }

  while (!code.empty()) {
    if (!ParseNextSection(&code, &section_info)) {
      return false;
    }

    if (section_info.section_id == kMerchantAccountInformationSectionId) {
      if (!ContainsValidSections(section_info.section_value)) {
        return false;
      }
      if (section_info.section_value.find(kPixCodeIndicator) != 0) {
        return false;
      }
      contains_pix_code_indicator = true;
    }

    if (section_info.section_id == kAdditionalDataFieldTemplateSectionId) {
      if (!ContainsValidSections(section_info.section_value)) {
        return false;
      }
    }

    if (code.empty() && section_info.section_id != kCrc16LastSectionId) {
      return false;
    }
  }

  return contains_pix_code_indicator;
}

}  // namespace payments::facilitated
