// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_CLIENT_H_
#define COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_CLIENT_H_

#include "base/functional/callback.h"

namespace optimization_guide {
class OptimizationGuideDecider;
class OptimizationGuideModelExecutor;
namespace proto {
class WalletablePass;
}  // namespace proto
}  // namespace optimization_guide

namespace strike_database {
class StrikeDatabaseBase;
}  // namespace strike_database

namespace wallet {

// A client interface that must be supplied to the Wallet component by the
// embedder (e.g., Chrome). The client's goal is to provide access to
// browser-level services required for walletable pass detection and extraction,
// such as the Optimization Guide. This allows the component to function without
// a direct dependency on the browser's implementation details.
//
// An implementation of this client is associated with a single tab and its
// lifecycle.
class WalletablePassClient {
 public:
  enum WalletablePassBubbleResult {
    kUnknown = 0,
    kLostFocus = 1,
    kClosed = 2,
    kAccepted = 3,
    kDeclined = 4,
    kMaxValue = kDeclined
  };

  using WalletablePassBubbleResultCallback =
      base::OnceCallback<void(WalletablePassBubbleResult)>;

  virtual ~WalletablePassClient() = default;

  virtual optimization_guide::OptimizationGuideDecider*
  GetOptimizationGuideDecider() = 0;

  virtual optimization_guide::OptimizationGuideModelExecutor*
  GetOptimizationGuideModelExecutor() = 0;

  virtual strike_database::StrikeDatabaseBase* GetStrikeDatabase() = 0;

  virtual void ShowWalletablePassConsentBubble(
      WalletablePassBubbleResultCallback callback) = 0;

  virtual void ShowWalletablePassSaveBubble(
      const optimization_guide::proto::WalletablePass& pass,
      WalletablePassBubbleResultCallback callback) = 0;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_CLIENT_H_
