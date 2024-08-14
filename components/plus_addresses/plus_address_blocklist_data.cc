// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_blocklist_data.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "components/plus_addresses/blocked_facets.pb.h"
#include "third_party/re2/src/re2/re2.h"

namespace plus_addresses {

namespace {

constexpr char kUmaKeyParsingResult[] = "PlusAddresses.Blocklist.ParsingResult";

std::unique_ptr<re2::RE2> ConstructRegex(std::string pattern) {
  if (pattern.empty()) {
    return nullptr;
  }

  re2::RE2::Options options;
  options.set_case_sensitive(false);
  return std::make_unique<re2::RE2>(std::move(pattern), options);
}
}  // namespace

// static
PlusAddressBlocklistData& PlusAddressBlocklistData::GetInstance() {
  static base::NoDestructor<PlusAddressBlocklistData> instance;
  return *instance;
}

PlusAddressBlocklistData::PlusAddressBlocklistData() = default;
PlusAddressBlocklistData::~PlusAddressBlocklistData() = default;

bool PlusAddressBlocklistData::PopulateDataFromComponent(
    const std::string& binary_pb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (binary_pb.empty()) {
    base::UmaHistogramEnumeration(
        kUmaKeyParsingResult,
        PlusAddressBlocklistDataParsingResult::kEmptyResponse);
    return false;
  }

  CompactPlusAddressBlockedFacets blocked_facets;
  if (!blocked_facets.ParseFromString(binary_pb)) {
    base::UmaHistogramEnumeration(
        kUmaKeyParsingResult,
        PlusAddressBlocklistDataParsingResult::kParsingError);
    return false;
  }

  exclusion_pattern_ = ConstructRegex(blocked_facets.exclusion_pattern());
  exception_pattern_ = ConstructRegex(blocked_facets.exception_pattern());

  base::UmaHistogramEnumeration(
      kUmaKeyParsingResult, PlusAddressBlocklistDataParsingResult::kSuccess);
  return true;
}

const re2::RE2* PlusAddressBlocklistData::GetExclusionPattern() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return exclusion_pattern_.get();
}

const re2::RE2* PlusAddressBlocklistData::GetExceptionPattern() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return exception_pattern_.get();
}

}  // namespace plus_addresses
