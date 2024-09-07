// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_METRICS_PLUS_ADDRESS_METRICS_H_
#define COMPONENTS_PLUS_ADDRESSES_METRICS_PLUS_ADDRESS_METRICS_H_

#include <string>

#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/plus_addresses/plus_address_types.h"

class GoogleServiceAuthError;

namespace plus_addresses::metrics {

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
// TODO(b/321060363) Re-evaluate metric if retry is enabled when error occurred.
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

// Logs plus address creation modal events triggered by user. `is_notice_screen`
// indicates whether the modal included the legal notice.
void RecordModalEvent(PlusAddressModalEvent plus_address_modal_event,
                      bool is_notice_screen);

// Logs plus address creation modal/bottom sheet shown duration for each
// closing `status`. `is_notice_screen` indicates whether the modal included the
// legal notice.
void RecordModalShownOutcome(PlusAddressModalCompletionStatus status,
                             base::TimeDelta modal_shown_duration,
                             int refresh_count,
                             bool is_notice_screen);

// Logs plus address autofill suggestion events.
void RecordAutofillSuggestionEvent(
    autofill::AutofillPlusAddressDelegate::SuggestionEvent
        plus_address_autofill_suggestion_event);

// Logs latency of a `type` of network request.
void RecordNetworkRequestLatency(PlusAddressNetworkRequestType type,
                                 base::TimeDelta request_latency);

// Logs the status code of the response to a `type` of network request.
void RecordNetworkRequestResponseCode(PlusAddressNetworkRequestType type,
                                      int response_code);

// Logs the size of the response to a `type` of network request.
void RecordNetworkRequestResponseSize(PlusAddressNetworkRequestType type,
                                      int response_size);

// Logs the OAuth errors that occur when PlusAddressHttpClient requests a
// token.
void RecordNetworkRequestOauthError(GoogleServiceAuthError error);

std::string PlusAddressNetworkRequestTypeToString(
    PlusAddressNetworkRequestType type);
std::string PlusAddressModalCompletionStatusToString(
    PlusAddressModalCompletionStatus status);

}  // namespace plus_addresses::metrics

#endif  // COMPONENTS_PLUS_ADDRESSES_METRICS_PLUS_ADDRESS_METRICS_H_
