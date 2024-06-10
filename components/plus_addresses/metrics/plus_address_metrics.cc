// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/metrics/plus_address_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/plus_addresses/plus_address_types.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace plus_addresses::metrics {

void RecordModalEvent(
    PlusAddressModalEvent plus_address_modal_event) {
  // TODO: crbug.com/346076280 - Remove this metric for M128.
  base::UmaHistogramEnumeration("Autofill.PlusAddresses.Modal.Events",
                                plus_address_modal_event);
  base::UmaHistogramEnumeration("PlusAddresses.Modal.Events",
                                plus_address_modal_event);
}

void RecordModalShownOutcome(
    PlusAddressModalCompletionStatus status,
    base::TimeDelta modal_shown_duration,
    int refresh_count) {
  // TODO: crbug.com/346076280 - Remove this metric for M128.
  base::UmaHistogramTimes(
      base::ReplaceStringPlaceholders(
          "Autofill.PlusAddresses.Modal.$1.ShownDuration",
          {PlusAddressModalCompletionStatusToString(status)},
          /*offsets=*/nullptr),
      modal_shown_duration);
  base::UmaHistogramTimes(
      base::ReplaceStringPlaceholders(
          "PlusAddresses.Modal.$1.ShownDuration",
          {PlusAddressModalCompletionStatusToString(status)},
          /*offsets=*/nullptr),
      modal_shown_duration);
  // TODO: crbug.com/346076280 - Remove this metric for M128.
  base::UmaHistogramExactLinear(
      base::ReplaceStringPlaceholders(
          "Autofill.PlusAddresses.Modal.$1.Refreshes",
          {PlusAddressModalCompletionStatusToString(status)},
          /*offsets=*/nullptr),
      refresh_count, /*exclusive_max=*/31);
  base::UmaHistogramExactLinear(
      base::ReplaceStringPlaceholders(
          "PlusAddresses.Modal.$1.Refreshes",
          {PlusAddressModalCompletionStatusToString(status)},
          /*offsets=*/nullptr),
      refresh_count, /*exclusive_max=*/31);
}

void RecordAutofillSuggestionEvent(
    autofill::AutofillPlusAddressDelegate::SuggestionEvent
        plus_address_autofill_suggestion_event) {
  // TODO: crbug.com/346076280 - Remove this metric for M128.
  base::UmaHistogramEnumeration("Autofill.PlusAddresses.Suggestion.Events",
                                plus_address_autofill_suggestion_event);
  base::UmaHistogramEnumeration("PlusAddresses.Suggestion.Events",
                                plus_address_autofill_suggestion_event);
}

void RecordNetworkRequestLatency(
    PlusAddressNetworkRequestType type,
    base::TimeDelta request_latency) {
  // TODO: crbug.com/346076280 - Remove this metric for M128.
  base::UmaHistogramTimes(
      base::ReplaceStringPlaceholders(
          "Autofill.PlusAddresses.NetworkRequest.$1.Latency",
          {PlusAddressNetworkRequestTypeToString(type)},
          /*offsets=*/nullptr),
      request_latency);
  base::UmaHistogramTimes(base::ReplaceStringPlaceholders(
                              "PlusAddresses.NetworkRequest.$1.Latency",
                              {PlusAddressNetworkRequestTypeToString(type)},
                              /*offsets=*/nullptr),
                          request_latency);
}

void RecordNetworkRequestResponseCode(
    PlusAddressNetworkRequestType type,
    int response_code) {
  // Mapped to "HttpErrorCodes" in histograms.xml.
  // TODO: crbug.com/346076280 - Remove this metric for M128.
  base::UmaHistogramSparse(
      base::ReplaceStringPlaceholders(
          "Autofill.PlusAddresses.NetworkRequest.$1.ResponseCode",
          {PlusAddressNetworkRequestTypeToString(type)},
          /*offsets=*/nullptr),
      response_code);
  base::UmaHistogramSparse(base::ReplaceStringPlaceholders(
                               "PlusAddresses.NetworkRequest.$1.ResponseCode",
                               {PlusAddressNetworkRequestTypeToString(type)},
                               /*offsets=*/nullptr),
                           response_code);
}

void RecordNetworkRequestResponseSize(
    PlusAddressNetworkRequestType type,
    int response_size) {
  // TODO: crbug.com/346076280 - Remove this metric for M128.
  base::UmaHistogramCounts10000(
      base::ReplaceStringPlaceholders(
          "Autofill.PlusAddresses.NetworkRequest.$1.ResponseByteSize",
          {PlusAddressNetworkRequestTypeToString(type)},
          /*offsets=*/nullptr),
      response_size);
  base::UmaHistogramCounts10000(
      base::ReplaceStringPlaceholders(
          "PlusAddresses.NetworkRequest.$1.ResponseByteSize",
          {PlusAddressNetworkRequestTypeToString(type)},
          /*offsets=*/nullptr),
      response_size);
}


void RecordNetworkRequestOauthError(
    GoogleServiceAuthError error) {
  // TODO: crbug.com/346076280 - Remove this metric for M128.
  base::UmaHistogramEnumeration(
      "Autofill.PlusAddresses.NetworkRequest.OauthError", error.state(),
      GoogleServiceAuthError::NUM_STATES);
  base::UmaHistogramEnumeration("PlusAddresses.NetworkRequest.OauthError",
                                error.state(),
                                GoogleServiceAuthError::NUM_STATES);
}

std::string PlusAddressNetworkRequestTypeToString(
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

std::string PlusAddressModalCompletionStatusToString(
    PlusAddressModalCompletionStatus status) {
  switch (status) {
    case PlusAddressModalCompletionStatus::kModalCanceled:
      return "Canceled";
    case PlusAddressModalCompletionStatus::kModalConfirmed:
      return "Confirmed";
    case PlusAddressModalCompletionStatus::kReservePlusAddressError:
      return "ReserveError";
    case PlusAddressModalCompletionStatus::kConfirmPlusAddressError:
      return "ConfirmError";
  }
}

}  // namespace plus_addresses::metrics
