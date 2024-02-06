// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/util/pix_code_validator.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/re2/src/re2/re2.h"

namespace payments::facilitated {
namespace {

constexpr char kSectionIdAndSizePattern[] = "\\d\\d\\d\\d";
constexpr char kPayloadFormatIndicatorFirstSectionId[] = "00";
constexpr char kMerchantAccountInformationSectionId[] = "26";
constexpr char kAdditionalDataFieldTemplateSectionId[] = "62";
constexpr char kCrc16LastSectionId[] = "63";
constexpr char kPixCodeIndicator[] = "0014br.gov.bcb.pix";

struct SectionInfo {
  std::string_view section_id;
  std::string_view section_value;
  std::string::size_type section_end = 0;
};

// Returns true if there is a valid PIX `code` section at the given `position`.
// A valid PIX code section comprises:
// 1) Two digits of section ID.
// 2) Two digits of section size |N|.
// 3) Section value of length |N|.
bool ParseNextSection(std::string::size_type position,
                      std::string_view code,
                      SectionInfo* section_info) {
  constexpr std::string::size_type kSectionIdAndSizeLength = 4;
  if (position + kSectionIdAndSizeLength > code.length()) {
    return false;
  }

  if (!re2::RE2::FullMatch(code.substr(position, kSectionIdAndSizeLength),
                           kSectionIdAndSizePattern)) {
    return false;
  }

  constexpr std::string::size_type kSectionIdLength = 2;
  section_info->section_id = code.substr(position, kSectionIdLength);

  constexpr std::string::size_type kSectionSizeLength = 2;
  std::string::size_type section_size = 0;
  if (!base::StringToSizeT(
          code.substr(position + kSectionIdLength, kSectionSizeLength),
          &section_size)) {
    // This number parsing is always successful because of the regex pattern
    // check above.
    NOTREACHED();
    return false;
  }

  section_info->section_end = position + kSectionIdAndSizeLength + section_size;
  if (section_info->section_end > code.length()) {
    return false;
  }

  section_info->section_value =
      code.substr(position + kSectionIdAndSizeLength, section_size);

  return true;
}

// Returns true if the `input` string consists of valid PIX code sections.
bool ContainsValidSections(std::string_view input) {
  if (input.empty()) {
    return false;
  }

  SectionInfo section_info;
  for (std::string::size_type position = 0; position < input.length();
       position = section_info.section_end) {
    if (!ParseNextSection(position, input, &section_info)) {
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
  bool is_first_section = true;
  bool contains_pix_code_indicator = false;

  for (std::string::size_type position = 0; position < code.length();
       position = section_info.section_end) {
    if (!ParseNextSection(position, code, &section_info)) {
      return false;
    }

    if (is_first_section) {
      is_first_section = false;
      if (section_info.section_id != kPayloadFormatIndicatorFirstSectionId) {
        return false;
      }
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

    if (section_info.section_end == code.length() &&
        section_info.section_id != kCrc16LastSectionId) {
      return false;
    }
  }

  return contains_pix_code_indicator;
}

}  // namespace payments::facilitated
