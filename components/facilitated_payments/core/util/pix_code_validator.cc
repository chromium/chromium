// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/util/pix_code_validator.h"

#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace payments::facilitated {
namespace {

constexpr char kSectionIdAndSizePattern[] = "(\\d{2})(\\d{2})";
constexpr char kPayloadFormatIndicatorFirstSectionId[] = "00";
constexpr char kMerchantAccountInformationSectionId[] = "26";
constexpr char kMerchantAccountInformationDynamicUrlSectionId[] = "25";
constexpr char kAdditionalDataFieldTemplateSectionId[] = "62";
constexpr char kCrc16LastSectionId[] = "63";
constexpr char kPixCodeIndicatorLowercase[] = "0014br.gov.bcb.pix";

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

PixCodeValidator::PixCodeValidator() = default;

PixCodeValidator::~PixCodeValidator() = default;

// static
bool PixCodeValidator::IsValidPixCode(std::string_view code) {
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
      if (base::ToLowerASCII(section_info.section_value)
              .find(kPixCodeIndicatorLowercase) != 0) {
        return false;
      }
      // By this time, we have already verified that the sub sections for
      // merchant account information are already valid. Only checking for the
      // presence of the dynamic url section id is sufficient to determine
      // whether the Pix code is a static vs dynamic code.
      SectionInfo dynamic_url_section_info;
      // We expect the dynamic url id to start right after the pix code
      // indicator.
      std::string_view dynamic_url_section_string =
          section_info.section_value.substr(strlen(kPixCodeIndicatorLowercase));
      ParseNextSection(&dynamic_url_section_string, &dynamic_url_section_info);
      if (dynamic_url_section_info.section_id !=
          kMerchantAccountInformationDynamicUrlSectionId) {
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

// static
bool PixCodeValidator::ContainsPixIdentifier(std::string_view code) {
  return base::ToLowerASCII(code).find(kPixCodeIndicatorLowercase) !=
         std::string::npos;
}

void PixCodeValidator::ValidatePixCode(
    const std::string& input_text,
    base::OnceCallback<void(std::optional<bool>)> callback) {
  std::move(callback).Run(IsValidPixCode(input_text));
}

}  // namespace payments::facilitated
