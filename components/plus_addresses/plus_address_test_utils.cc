// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_test_utils.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"

namespace plus_addresses::test {

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
    absl::optional<base::Value::Dict> dict = base::JSONReader::ReadDict(json);
    DCHECK(dict.has_value());
    list.Append(std::move(dict.value()));
  }
  response.Set("plusProfiles", std::move(list));
  absl::optional<std::string> json = base::WriteJson(response);
  DCHECK(json.has_value());
  return json.value();
}

std::string MakePlusProfile(const PlusProfile& profile) {
  // Note: the below must be kept in-line with the PlusAddressParser behavior.
  std::string mode = profile.is_confirmed ? "anyMode" : "UNSPECIFIED";
  std::string json = base::ReplaceStringPlaceholders(
      R"(
          {
            "facet": "$1",
            "plusEmail": {
              "plusAddress": "$2",
              "plusMode": "$3"
            }
          }
        )",
      {profile.facet, profile.plus_address, mode}, nullptr);
  DCHECK(base::JSONReader::Read(json));
  return json;
}

}  // namespace plus_addresses::test
