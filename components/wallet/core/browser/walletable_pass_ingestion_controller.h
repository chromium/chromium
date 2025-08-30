// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_INGESTION_CONTROLLER_H_
#define COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_INGESTION_CONTROLLER_H_

#include "base/memory/raw_ptr.h"

class GURL;

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace wallet {

// Controls the detection of walletable passes on a web page.
class WalletablePassIngestionController {
 public:
  explicit WalletablePassIngestionController(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  virtual ~WalletablePassIngestionController();

  // Not copyable or movable.
  WalletablePassIngestionController(const WalletablePassIngestionController&) =
      delete;
  WalletablePassIngestionController& operator=(
      const WalletablePassIngestionController&) = delete;

  // Checks if the URL is eligible for pass extraction. This is determined by
  // consulting an allowlist managed by the Optimization Guide.
  bool IsEligibleForExtraction(const GURL& url) const;

 protected:
  // Registers optimization types with the Optimization Guide to query the pass
  // extraction allowlist.
  void RegisterOptimizationTypes();

 private:
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_INGESTION_CONTROLLER_H_
