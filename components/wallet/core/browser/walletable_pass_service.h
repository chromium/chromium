// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_SERVICE_H_
#define COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace wallet {

// A cross-platform keyed service that used to run critical tasks for Walletable
// pass extraction.
class WalletablePassService : public KeyedService {
 public:
  explicit WalletablePassService(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  WalletablePassService(const WalletablePassService&) = delete;
  WalletablePassService& operator=(const WalletablePassService&) = delete;
  ~WalletablePassService() override;

  bool IsEligibleForExtraction(const GURL& url) const;

 private:
  void RegisterOptimizationTypes();

  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_SERVICE_H_
