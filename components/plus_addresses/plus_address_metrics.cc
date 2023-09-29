// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "components/plus_addresses/plus_address_types.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace plus_addresses {
// static
void PlusAddressMetrics::RecordModalEvent(
    PlusAddressModalEvent plus_address_modal_event) {
  base::UmaHistogramEnumeration("Autofill.PlusAddresses.Modal.Events",
                                plus_address_modal_event);
}

// static
void PlusAddressMetrics::RecordAutofillSuggestionEvent(
    PlusAddressAutofillSuggestionEvent plus_address_autofill_suggestion_event) {
  base::UmaHistogramEnumeration("Autofill.PlusAddresses.Suggestion.Events",
                                plus_address_autofill_suggestion_event);
}
// static
void PlusAddressMetrics::RecordNetworkRequestLatency(
    PlusAddressNetworkRequestType type,
    base::TimeDelta request_latency) {
  base::UmaHistogramTimes(
      base::ReplaceStringPlaceholders(
          "Autofill.PlusAddresses.NetworkRequest.$1.Latency",
          {PlusAddressNetworkRequestTypeToString(type)},
          /*offsets=*/nullptr),
      request_latency);
}
// static
void PlusAddressMetrics::RecordNetworkRequestResponseCode(
    PlusAddressNetworkRequestType type,
    int response_code) {
  // Mapped to "HttpErrorCodes" in histograms.xml.
  base::UmaHistogramSparse(
      base::ReplaceStringPlaceholders(
          "Autofill.PlusAddresses.NetworkRequest.$1.ResponseCode",
          {PlusAddressNetworkRequestTypeToString(type)},
          /*offsets=*/nullptr),
      response_code);
}
// static
void PlusAddressMetrics::RecordNetworkRequestResponseSize(
    PlusAddressNetworkRequestType type,
    int response_size) {
  base::UmaHistogramCounts10000(
      base::ReplaceStringPlaceholders(
          "Autofill.PlusAddresses.NetworkRequest.$1.ResponseByteSize",
          {PlusAddressNetworkRequestTypeToString(type)},
          /*offsets=*/nullptr),
      response_size);
}
// static
void PlusAddressMetrics::RecordNetworkRequestOauthError(
    GoogleServiceAuthError error) {
  base::UmaHistogramEnumeration(
      "Autofill.PlusAddresses.NetworkRequest.OauthError", error.state(),
      GoogleServiceAuthError::NUM_STATES);
}
// static
std::string PlusAddressMetrics::PlusAddressNetworkRequestTypeToString(
    PlusAddressNetworkRequestType type) {
  // Make sure to keep "AutofillPlusAddressNetworkRequestType" in
  // tools/metrics/histograms/metadata/autofill/histograms.xml in sync with
  // this.
  switch (type) {
    case PlusAddressNetworkRequestType::kCreate:
      return "Create";
    case PlusAddressNetworkRequestType::kGetOrCreate:
      return "GetOrCreate";
    case PlusAddressNetworkRequestType::kList:
      return "List";
    case PlusAddressNetworkRequestType::kReserve:
      return "Reserve";
  }
}
}  // namespace plus_addresses
