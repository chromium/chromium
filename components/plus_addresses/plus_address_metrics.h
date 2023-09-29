// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_METRICS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_METRICS_H_

#include "base/time/time.h"
#include "components/plus_addresses/plus_address_types.h"

class GoogleServiceAuthError;

namespace plus_addresses {
// A small, stateless utility class for logging metrics. It will handle autofill
// metrics, plus_address_service metrics, and user interaction metrics.
class PlusAddressMetrics {
 public:
  enum class PlusAddressModalEvent {
    kModalShown = 0,
    kModalCanceled = 1,
    kModalConfirmed = 2,
    kMaxValue = kModalConfirmed,
  };

  enum class PlusAddressAutofillSuggestionEvent {
    kExistingPlusAddressSuggested = 0,
    kCreateNewPlusAddressSuggested = 1,
    kExistingPlusAddressChosen = 2,
    kCreateNewPlusAddressChosen = 3,
    kMaxValue = kCreateNewPlusAddressChosen,
  };

  // As of now, the class is intended to be stateless and static; do not allow
  // construction.
  PlusAddressMetrics() = delete;
  PlusAddressMetrics(const PlusAddressMetrics&) = delete;
  PlusAddressMetrics& operator=(const PlusAddressMetrics&) = delete;

  // Log plus address creation modal events.
  static void RecordModalEvent(PlusAddressModalEvent plus_address_modal_event);

  // Log plus address autofill suggestion events.
  static void RecordAutofillSuggestionEvent(
      PlusAddressAutofillSuggestionEvent
          plus_address_autofill_suggestion_event);
  // Log latency of `request`.
  static void RecordNetworkRequestLatency(PlusAddressNetworkRequestType type,
                                          base::TimeDelta request_latency);
  // Log the status code of the response to `request`.
  static void RecordNetworkRequestResponseCode(
      PlusAddressNetworkRequestType type,
      int response_code);
  // Log the size of the response to `request`.
  static void RecordNetworkRequestResponseSize(
      PlusAddressNetworkRequestType type,
      int response_size);
  // Log the OAuth errors that occur when PlusAddressClient requests a token.
  static void RecordNetworkRequestOauthError(GoogleServiceAuthError error);
  static std::string PlusAddressNetworkRequestTypeToString(
      PlusAddressNetworkRequestType type);
};
}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_METRICS_H_
