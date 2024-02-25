// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_UTILS_H_

#include "components/commerce/core/proto/parcel.pb.h"

namespace commerce {

// Network request type for parcels service.
enum class ParcelRequestType {
  kUnknown = 0,
  kStartTrackingParcels = 1,
  kGetParcelStatus = 2,
  kStopTrackingParcels = 3,
  kStopTrackingAllParcels = 4,
  kMaxValue = kStopTrackingAllParcels,
};

// Possible result status of a parcel tracking request.
// TODO(qinmin): emit histogram with these enums. And merge these
// enums with the ones defined in SubscriptionsManager.
enum class ParcelRequestStatus {
  // Subscriptions successfully added or removed on server.
  kSuccess = 0,
  // Parcel identifiers are invalid, missing tracking id or carrier.
  kInvalidParcelIdentifiers = 1,
  // Server failed to process the request, e.g. the request is invalid or
  // network error.
  kServerError = 2,
  // Error parsing server response, the response may be malformed.
  kServerReponseParsingError = 3,
  // This enum must be last and is only used for histograms.
  kMaxValue = kServerReponseParsingError,
};

bool IsParcelStateDone(ParcelStatus::ParcelState parcel_state);

void RecordParcelsRequestMetrics(ParcelRequestType request_type,
                                 ParcelRequestStatus request_status);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PARCEL_PARCELS_UTILS_H_
