// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PARSING_UTILS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PARSING_UTILS_H_

#include <optional>

#include "base/values.h"
#include "components/plus_addresses/plus_address_types.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace plus_addresses {

// Utilities for parsing plus-address responses from a remote plus address
// server.

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
std::optional<PlusProfile> ParsePlusProfileFromV1Create(
    data_decoder::DataDecoder::ValueOrError response);

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
std::optional<PlusAddressMap> ParsePlusAddressMapFromV1List(
    data_decoder::DataDecoder::ValueOrError response);

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PARSING_UTILS_H_
