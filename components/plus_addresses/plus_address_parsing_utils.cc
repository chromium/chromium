// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_parsing_utils.h"

#include <optional>
#include <utility>

#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/webdata/plus_address_webdata_service.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace plus_addresses {

namespace {

// Creates a PlusProfile for `dict` if it fits this schema (in TS notation):
// {
//   "ProfileId": string,
//   "facet": string,
//   "plusEmail": {
//     "plusAddress": string,
//     "plusMode": string,
//   }
// }
// Returns nullopt if none of the values are parsed.
std::optional<PlusProfile> ParsePlusProfileFromV1Dict(base::Value::Dict dict) {
  std::string profile_id;
  std::string facet_str;
  std::string plus_address;
  std::optional<bool> is_confirmed;
  for (std::pair<const std::string&, base::Value&> entry : dict) {
    auto [key, val] = entry;
    if (base::MatchPattern(key, "*ProfileId") && val.is_string()) {
      profile_id = std::move(val.GetString());
      continue;
    }
    if (base::MatchPattern(key, "facet") && val.is_string()) {
      facet_str = std::move(val.GetString());
      continue;
    }
    if (base::MatchPattern(key, "*Email") && val.is_dict()) {
      for (std::pair<const std::string&, base::Value&> email_entry :
           val.GetDict()) {
        auto [email_key, email_val] = email_entry;
        if (!email_val.is_string()) {
          continue;
        }
        if (base::MatchPattern(email_key, "*Address")) {
          plus_address = std::move(email_val.GetString());
        }
        if (base::MatchPattern(email_key, "*Mode")) {
          is_confirmed =
              !base::MatchPattern(email_val.GetString(), "*UNSPECIFIED");
        }
      }
    }
  }
  if (profile_id.empty() || facet_str.empty() || plus_address.empty() ||
      !is_confirmed.has_value()) {
    return std::nullopt;
  }
  if (!IsSyncingPlusAddresses()) {
    return PlusProfile(std::move(profile_id), std::move(facet_str),
                       std::move(plus_address), *is_confirmed);
  }
  affiliations::FacetURI facet =
      affiliations::FacetURI::FromPotentiallyInvalidSpec(facet_str);
  if (!facet.is_valid()) {
    return std::nullopt;
  }
  return PlusProfile(std::move(profile_id), std::move(facet),
                     std::move(plus_address), *is_confirmed);
}

// Creates a list of PlusProfiles by parsing each dict-value in `list` that
// fits this schema (in TS notation):
// {
//   "facet": string,
//   "plusEmail": {
//     "plusAddress": string
//     "plusMode": string,
//   }
// }[]
// The returned list only contains PlusProfiles which could be parsed.
std::vector<PlusProfile> ParsePlusProfilesFromV1ProfileList(
    base::Value::List list) {
  std::vector<PlusProfile> profiles;
  profiles.reserve(list.size());
  for (base::Value& entry : list) {
    if (!entry.is_dict()) {
      continue;
    }
    if (std::optional<PlusProfile> maybe_profile =
            ParsePlusProfileFromV1Dict(std::move(entry.GetDict()))) {
      profiles.push_back(std::move(*maybe_profile));
    }
  }
  return profiles;
}

}  // namespace

std::optional<PlusProfile> ParsePlusProfileFromV1Create(
    data_decoder::DataDecoder::ValueOrError response) {
  if (!response.has_value() || !response->is_dict()) {
    return std::nullopt;
  }

  // Use iterators to avoid looking up by JSON keys.
  for (std::pair<const std::string&, base::Value&> first_level_entry :
       response->GetDict()) {
    auto [first_key, first_val] = first_level_entry;
    if (base::MatchPattern(first_key, "*Profile") && first_val.is_dict()) {
      return ParsePlusProfileFromV1Dict(std::move(first_val.GetDict()));
    }
  }
  return std::nullopt;
}

std::optional<PlusAddressMap> ParsePlusAddressMapFromV1List(
    data_decoder::DataDecoder::ValueOrError response) {
  if (!response.has_value() || !response->is_dict()) {
    return std::nullopt;
  }
  // Use iterators to avoid looking up by JSON keys.
  for (std::pair<const std::string&, base::Value&> first_level_entry :
       response->GetDict()) {
    auto [first_key, first_val] = first_level_entry;
    if (base::MatchPattern(first_key, "*Profiles") && first_val.is_list()) {
      PlusAddressMap site_to_plus_address;
      // Parse the list of profiles and add the result to the mapping.
      for (PlusProfile& profile :
           ParsePlusProfilesFromV1ProfileList(std::move(first_val.GetList()))) {
        site_to_plus_address[std::move(absl::get<std::string>(profile.facet))] =
            std::move(profile.plus_address);
      }
      return site_to_plus_address;
    }
  }
  // Return nullopt if the `*Profiles` key is not present.
  return std::nullopt;
}

}  // namespace plus_addresses
