// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/metrics/wallet_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/wallet/core/browser/data_models/data_model_utils.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace wallet::metrics {

void LogOptInEvent(PassCategory pass_category,
                   WalletablePassOptInFunnelEvents event) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Wallet.WalletablePass.OptIn.Funnel.",
                    PassCategoryToString(pass_category)}),
      event);
}

void LogServerExtractionEvent(
    PassCategory pass_category,
    WalletablePassServerExtractionFunnelEvents event) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Wallet.WalletablePass.ServerExtraction.Funnel.",
                    PassCategoryToString(pass_category)}),
      event);
}

void LogSaveEvent(PassCategory pass_category,
                  WalletablePassSaveFunnelEvents event) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Wallet.WalletablePass.Save.Funnel.",
                    PassCategoryToString(pass_category)}),
      event);
}

void RecordNetworkRequestOauthError(const GoogleServiceAuthError& error) {
  base::UmaHistogramEnumeration("Wallet.NetworkRequest.OauthError",
                                error.state(),
                                GoogleServiceAuthError::NUM_STATES);
}

void RecordHttpResponseOrErrorCode(WalletRequest::WalletNetworkRequestType type,
                                   int http_response_or_net_error) {
  base::UmaHistogramSparse(
      base::ReplaceStringPlaceholders(
          "Wallet.NetworkRequest.$1.HttpResponseOrErrorCode",
          {WalletNetworkRequestTypeToString(type)},
          /*offsets=*/nullptr),
      http_response_or_net_error);
}

void RecordNetworkRequestLatency(WalletRequest::WalletNetworkRequestType type,
                                 base::TimeDelta request_latency) {
  base::UmaHistogramTimes(
      base::ReplaceStringPlaceholders("Wallet.NetworkRequest.$1.Latency",
                                      {WalletNetworkRequestTypeToString(type)},
                                      /*offsets=*/nullptr),
      request_latency);
}

void RecordNetworkRequestResponseSize(
    WalletRequest::WalletNetworkRequestType type,
    size_t response_size) {
  base::UmaHistogramCounts10000(base::ReplaceStringPlaceholders(
                                    "Wallet.NetworkRequest.$1.ResponseByteSize",
                                    {WalletNetworkRequestTypeToString(type)},
                                    /*offsets=*/nullptr),
                                response_size);
}

std::string WalletNetworkRequestTypeToString(
    WalletRequest::WalletNetworkRequestType type) {
  switch (type) {
    case WalletRequest::WalletNetworkRequestType::kUpsertPass:
      return "UpsertPass";
    case WalletRequest::WalletNetworkRequestType::kUpsertPrivatePass:
      return "UpsertPrivatePass";
    case WalletRequest::WalletNetworkRequestType::kGetUnmaskedPrivatePass:
      return "GetUnmaskedPrivatePass";
  }
}

}  // namespace wallet::metrics
