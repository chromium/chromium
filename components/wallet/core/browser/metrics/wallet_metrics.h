// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_METRICS_WALLET_METRICS_H_
#define COMPONENTS_WALLET_CORE_BROWSER_METRICS_WALLET_METRICS_H_

#include "components/wallet/core/browser/data_models/wallet_pass.h"
#include "components/wallet/core/browser/network/wallet_request.h"

namespace base {
class TimeDelta;
}

class GoogleServiceAuthError;

namespace wallet::metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(WalletablePassOptInFunnelEvents)
enum class WalletablePassOptInFunnelEvents {
  kConsentBubbleWasBlockedByStrike = 0,
  kUserAlreadyOptedIn = 1,
  kConsentBubbleWasShown = 2,
  kLearnMoreButtonClicked = 3,
  kConsentBubbleWasAccepted = 4,
  kConsentBubbleWasRejected = 5,
  kConsentBubbleWasClosed = 6,
  kConsentBubbleLostFocus = 7,
  kConsentBubbleClosedUnknownReason = 8,
  kConsentBubbleWasDiscarded = 9,
  kMaxValue = kConsentBubbleWasDiscarded,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/wallet/enums.xml:WalletablePassOptInFunnelEvents)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(WalletablePassServerExtractionFunnelEvents)
enum class WalletablePassServerExtractionFunnelEvents {
  kExtractionBlockedBySaveStrike = 0,
  kGetAnnotatedPageContentFailed = 1,
  kModelExecutionFailed = 2,
  kResponseCannotBeParsed = 3,
  kNoPassExtracted = 4,
  kInvalidPassType = 5,
  kWalletablePassConversionFailed = 6,
  kExtractionSucceeded = 7,
  kMaxValue = kExtractionSucceeded,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/wallet/enums.xml:WalletablePassServerExtractionFunnelEvents)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(WalletablePassSaveFunnelEvents)
enum class WalletablePassSaveFunnelEvents {
  kSaveBubbleWasShown = 0,
  kGoToWalletButtonClicked = 1,
  kSaveBubbleWasAccepted = 2,
  kSaveBubbleWasRejected = 3,
  kSaveBubbleWasClosed = 4,
  kSaveBubbleLostFocus = 5,
  kSaveBubbleClosedUnknownReason = 6,
  kSaveBubbleWasDiscarded = 7,
  kMaxValue = kSaveBubbleWasDiscarded,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/wallet/enums.xml:WalletablePassSaveFunnelEvents)

void LogOptInEvent(PassCategory pass_category,
                   WalletablePassOptInFunnelEvents event);

void LogServerExtractionEvent(PassCategory pass_category,
                              WalletablePassServerExtractionFunnelEvents event);

void LogSaveEvent(PassCategory pass_category,
                  WalletablePassSaveFunnelEvents event);

// Logs the OAuth errors that occur when WalletHttpClient requests a token.
void RecordNetworkRequestOauthError(const GoogleServiceAuthError& error);

// Logs the net error code that occur when WalletHttpClient makes a network
// request.
void RecordHttpResponseOrErrorCode(WalletRequest::WalletNetworkRequestType type,
                                   int http_response_or_net_error);

// Logs latency of a `type` of network request.
void RecordNetworkRequestLatency(WalletRequest::WalletNetworkRequestType type,
                                 base::TimeDelta request_latency);

// Logs the size of the response to a `type` of network request.
void RecordNetworkRequestResponseSize(
    WalletRequest::WalletNetworkRequestType type,
    size_t response_size);

std::string WalletNetworkRequestTypeToString(
    WalletRequest::WalletNetworkRequestType type);

}  // namespace wallet::metrics

#endif  // COMPONENTS_WALLET_CORE_BROWSER_METRICS_WALLET_METRICS_H_
