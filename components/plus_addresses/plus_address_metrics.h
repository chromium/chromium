// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_METRICS_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_METRICS_H_

#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/plus_addresses/plus_address_types.h"

class GoogleServiceAuthError;

namespace plus_addresses {
// A small, stateless utility class for logging metrics. It will handle autofill
// metrics, plus_address_service metrics, and user interaction metrics.
class PlusAddressMetrics {
 public:
  // `PlusAddressModalEvent` is for categorizing user interactions with the
  // modal/bottom sheet.
  enum class PlusAddressModalEvent {
    kModalShown = 0,
    kModalCanceled = 1,
    kModalConfirmed = 2,
    kMaxValue = kModalConfirmed,
    // TODO(b/320541525) Expand record of user events once user flow becomes
    // more complex.
  };

  // `PlusAddressModalCompletionStatus` indicates the reason modal/bottom sheet
  // is dismissed.
  // TODO(b/321060363) Re-evaluate metric if retry is enable when error occur.
  enum class PlusAddressModalCompletionStatus {
    // User cancels the modal (independent of any error).
    kModalCanceled = 0,
    // User successfully confirm plus address.
    kModalConfirmed = 1,
    // User cancels the modal after error occur during plus address reservation
    // (cancel is the only option).
    kReservePlusAddressError = 2,
    // User cancels the modal after error occur during plus address confirmation
    // (user has pressed confirm button).
    kConfirmPlusAddressError = 3,
    kMaxValue = kConfirmPlusAddressError,
  };

  // As of now, the class is intended to be stateless and static; do not allow
  // construction.
  PlusAddressMetrics() = delete;
  PlusAddressMetrics(const PlusAddressMetrics&) = delete;
  PlusAddressMetrics& operator=(const PlusAddressMetrics&) = delete;

  // Log plus address creation modal events triggered by user.
  static void RecordModalEvent(PlusAddressModalEvent plus_address_modal_event);
  // Log plus address creation modal/bottom sheet shown duration for each
  // closing `status`.
  static void RecordModalShownDuration(PlusAddressModalCompletionStatus status,
                                       base::TimeDelta modal_shown_duration);
  // Log plus address autofill suggestion events.
  static void RecordAutofillSuggestionEvent(
      autofill::AutofillPlusAddressDelegate::SuggestionEvent
          plus_address_autofill_suggestion_event);
  // Log latency of a `type` of network request.
  static void RecordNetworkRequestLatency(PlusAddressNetworkRequestType type,
                                          base::TimeDelta request_latency);
  // Log the status code of the response to a `type` of network request.
  static void RecordNetworkRequestResponseCode(
      PlusAddressNetworkRequestType type,
      int response_code);
  // Log the size of the response to a `type` of network request.
  static void RecordNetworkRequestResponseSize(
      PlusAddressNetworkRequestType type,
      int response_size);
  // Log the OAuth errors that occur when PlusAddressHttpClient requests a
  // token.
  static void RecordNetworkRequestOauthError(GoogleServiceAuthError error);
  static std::string PlusAddressNetworkRequestTypeToString(
      PlusAddressNetworkRequestType type);
  static std::string PlusAddressModalCompletionStatusToString(
      PlusAddressModalCompletionStatus status);
};
}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_METRICS_H_
