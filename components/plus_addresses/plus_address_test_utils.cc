// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_test_utils.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "components/affiliations/core/browser/affiliation_utils.h"

namespace plus_addresses::test {

PlusProfile CreatePlusProfile(bool use_full_domain) {
  PlusProfile::facet_t facet;
  if (use_full_domain) {
    facet = affiliations::FacetURI::FromCanonicalSpec("https://foo.com");
  } else {
    facet = "foo.com";
  }
  return PlusProfile(/*profile_id=*/"123", facet,
                     /*plus_address=*/"plus+foo@plus.plus",
                     /*is_confirmed=*/true);
}

PlusProfile CreatePlusProfile2(bool use_full_domain) {
  PlusProfile::facet_t facet;
  if (use_full_domain) {
    facet = affiliations::FacetURI::FromCanonicalSpec("https://bar.com");
  } else {
    facet = "bar.com";
  }
  return PlusProfile(/*profile_id=*/"234", facet,
                     /*plus_address=*/"plus+bar@plus.plus",
                     /*is_confirmed=*/true);
}

PlusProfile CreatePlusProfileWithFacet(const affiliations::FacetURI& facet) {
  PlusProfile profile = CreatePlusProfile();
  profile.facet = facet;
  return profile;
}

std::string MakeCreationResponse(const PlusProfile& profile) {
  std::string json = base::ReplaceStringPlaceholders(
      R"(
          {
            "plusProfile": $1
          }
        )",
      {MakePlusProfile(profile)}, nullptr);
  DCHECK(base::JSONReader::Read(json));
  return json;
}

std::string MakeListResponse(const std::vector<PlusProfile>& profiles) {
  base::Value::Dict response;
  base::Value::List list;
  for (const PlusProfile& profile : profiles) {
    std::string json = MakePlusProfile(profile);
    std::optional<base::Value::Dict> dict = base::JSONReader::ReadDict(json);
    DCHECK(dict.has_value());
    list.Append(std::move(dict.value()));
  }
  response.Set("plusProfiles", std::move(list));
  std::optional<std::string> json = base::WriteJson(response);
  DCHECK(json.has_value());
  return json.value();
}

std::string MakePlusProfile(const PlusProfile& profile) {
  // Note: the below must be kept in-line with the PlusAddressParser behavior.
  std::string mode = profile.is_confirmed ? "anyMode" : "UNSPECIFIED";
  std::string json = base::ReplaceStringPlaceholders(
      R"(
          {
            "ProfileId": "$1",
            "facet": "$2",
            "plusEmail": {
              "plusAddress": "$3",
              "plusMode": "$4"
            }
          }
        )",
      {profile.profile_id, absl::get<std::string>(profile.facet),
       profile.plus_address, mode},
      nullptr);
  DCHECK(base::JSONReader::Read(json));
  return json;
}

}  // namespace plus_addresses::test
