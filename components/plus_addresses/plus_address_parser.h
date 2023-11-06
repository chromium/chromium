// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PARSER_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PARSER_H_

#include "base/values.h"
#include "components/plus_addresses/plus_address_types.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace plus_addresses {

// Utility for parsing plus-address responses from a remote plus-address server.
class PlusAddressParser {
 public:
  //   If `response` is present, it should fit this schema (in TS notation):
  //   {
  //     "plusProfile":
  //       {
  //         "facet": string,
  //         "plusEmail": {
  //           "plusAddress": string,
  //           "plusMode": string,
  //         }
  //       }
  //   }
  //  This method returns nullopt otherwise or if `response` is an error.
  static absl::optional<PlusProfile> ParsePlusProfileFromV1Create(
      const data_decoder::DataDecoder::ValueOrError response);

  //   If `response` is present, it should fit this schema (in TS notation):
  //   {
  //     "plusProfiles":
  //       {
  //         "facet": string,
  //         "plusEmail": {
  //           "plusAddress": string,
  //           "plusMode": string,
  //         }
  //       }[]
  //   }
  //  This method returns nullopt otherwise or if `response` is an error.
  //
  // Note: `plusProfiles` may have 0 or many profiles. The "plusProfiles" key
  // must always be present though.
  static absl::optional<PlusAddressMap> ParsePlusAddressMapFromV1List(
      const data_decoder::DataDecoder::ValueOrError response);

 private:
  // Create a PlusProfile for `dict` if it fits this schema (in TS notation):
  // {
  //   "facet": string,
  //   "plusEmail": {
  //     "plusAddress": string,
  //     "plusMode": string,
  //   }
  // }
  // Returns nullopt if none of the values are parsed.
  static absl::optional<PlusProfile> ParsePlusProfileFromV1Dict(
      const base::Value::Dict* dict);

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
  static std::vector<PlusProfile> ParsePlusProfilesFromV1ProfileList(
      const base::Value::List* list);
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PARSER_H_
