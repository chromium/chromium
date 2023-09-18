// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_parser.h"

#include "base/strings/pattern.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace plus_addresses {

// static
absl::optional<std::string> PlusAddressParser::ParsePlusAddressFromV1Create(
    const data_decoder::DataDecoder::ValueOrError response) {
  if (!response.has_value() || !response->is_dict()) {
    return absl::nullopt;
  }

  // Use iterators to avoid looking up by JSON keys.
  for (const std::pair<const std::string&, const base::Value&>
           first_level_entry : response.value().GetDict()) {
    const auto& [first_key, first_val] = first_level_entry;
    if (base::MatchPattern(first_key, "*Profile") && first_val.is_dict()) {
      const absl::optional<PlusProfile> profile =
          ParsePlusProfileFromV1Dict(first_val.GetIfDict());
      return profile.has_value() ? absl::make_optional(profile->plus_address)
                                 : absl::nullopt;
    }
  }
  return absl::nullopt;
}

// static
absl::optional<PlusAddressMap> PlusAddressParser::ParsePlusAddressMapFromV1List(
    const data_decoder::DataDecoder::ValueOrError response) {
  if (!response.has_value() || !response->is_dict()) {
    return absl::nullopt;
  }
  // Use iterators to avoid looking up by JSON keys.
  for (const std::pair<const std::string&, const base::Value&>
           first_level_entry : response.value().GetDict()) {
    const auto& [first_key, first_val] = first_level_entry;
    if (base::MatchPattern(first_key, "*Profiles") && first_val.is_list()) {
      PlusAddressMap site_to_plus_address;
      // Parse the list of profiles and add the result to the mapping.
      const std::vector<PlusProfile> profiles =
          ParsePlusProfilesFromV1ProfileList(first_val.GetIfList());
      for (const PlusProfile& profile : profiles) {
        site_to_plus_address[profile.facet] = profile.plus_address;
      }
      return absl::make_optional(site_to_plus_address);
    }
  }
  // Only return nullopt if the `plusProfiles` key is not present.
  return absl::nullopt;
}

// static
absl::optional<PlusAddressParser::PlusProfile>
PlusAddressParser::ParsePlusProfileFromV1Dict(const base::Value::Dict* dict) {
  if (!dict) {
    return absl::nullopt;
  }

  std::string facet, plus_address;
  for (const std::pair<const std::string&, const base::Value&> entry : *dict) {
    const auto& [key, val] = entry;
    if (base::MatchPattern(key, "facet") && val.is_string()) {
      facet = val.GetString();
      continue;
    }
    if (base::MatchPattern(key, "*Email") && val.is_dict()) {
      for (const std::pair<const std::string&, const base::Value&> email_entry :
           val.GetDict()) {
        const auto& [email_key, email_val] = email_entry;
        if (base::MatchPattern(email_key, "*Address") &&
            email_val.is_string()) {
          plus_address = email_val.GetString();
        }
      }
    }
  }
  if (facet.empty() || plus_address.empty()) {
    return absl::nullopt;
  }
  return absl::make_optional(
      PlusProfile{.facet = facet, .plus_address = plus_address});
}

// static
std::vector<PlusAddressParser::PlusProfile>
PlusAddressParser::ParsePlusProfilesFromV1ProfileList(
    const base::Value::List* list) {
  if (!list) {
    return {};
  }
  std::vector<PlusProfile> profiles;
  for (const base::Value& entry : *list) {
    absl::optional<PlusProfile> maybe_profile =
        ParsePlusProfileFromV1Dict(entry.GetIfDict());
    if (maybe_profile) {
      profiles.push_back(maybe_profile.value());
    }
  }
  return profiles;
}

}  // namespace plus_addresses
