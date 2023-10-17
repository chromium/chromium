// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/commerce/core/parcel/parcels_utils.h"

namespace commerce {

namespace {

const char kParcelTrackingHistogramNameFormat[] =
    "Commerce.ParcelTracking.%s.RequestStatus";

std::string GetParcelRequestTypeName(ParcelRequestType request_type) {
  switch (request_type) {
    case ParcelRequestType::kStartTrackingParcels:
      return "StartTrackingParcels";
    case ParcelRequestType::kGetParcelStatus:
      return "GetParcelStatus";
    case ParcelRequestType::kStopTrackingParcels:
      return "StopTrackingParcels";
    case ParcelRequestType::kStopTrackingAllParcels:
      return "StopTrackingAllParcels";
    default:
      return "Unknown";
  }
}

}  // namespace

bool IsParcelStateDone(ParcelStatus::ParcelState parcel_state) {
#pragma push_macro("ERROR")
#undef ERROR
  switch (parcel_state) {
    case ParcelStatus::FINISHED:
    case ParcelStatus::ERROR:
    case ParcelStatus::CANCELLED:
    case ParcelStatus::ORDER_TOO_OLD:
    case ParcelStatus::RETURN_COMPLETED:
    case ParcelStatus::UNDELIVERABLE:
      return true;
    default:
      return false;
  }
#pragma pop_macro("ERROR")
}

void RecordParcelsRequestMetrics(ParcelRequestType request_type,
                                 ParcelRequestStatus request_status) {
  std::string histogram_name =
      base::StringPrintf(kParcelTrackingHistogramNameFormat,
                         GetParcelRequestTypeName(request_type).c_str());
  base::UmaHistogramEnumeration(histogram_name, request_status);
}

}  // namespace commerce
