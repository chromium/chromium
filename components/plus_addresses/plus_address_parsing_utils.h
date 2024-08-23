// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PARSING_UTILS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PARSING_UTILS_H_

#include <optional>
#include <vector>

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

// Attempts to parse `response` into a vector of `PreallocatedPlusAddress`.
// The following schema is expected:
// {
//    "emailAddresses":
//    {
//      "plus_address": string
//      "lifetime": string of format [0-9]s.
//    } []
// }
//
std::optional<std::vector<PreallocatedPlusAddress>>
ParsePreallocatedPlusAddresses(
    data_decoder::DataDecoder::ValueOrError response);

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PARSING_UTILS_H_
