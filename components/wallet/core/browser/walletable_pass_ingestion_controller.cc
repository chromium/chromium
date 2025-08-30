// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/walletable_pass_ingestion_controller.h"

#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "url/gurl.h"

namespace wallet {

WalletablePassIngestionController::WalletablePassIngestionController(
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : optimization_guide_decider_(optimization_guide_decider) {
  RegisterOptimizationTypes();
}

WalletablePassIngestionController::~WalletablePassIngestionController() =
    default;

bool WalletablePassIngestionController::IsEligibleForExtraction(
    const GURL& url) const {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // Check if URL is allowlisted via optimization guide
  return optimization_guide_decider_->CanApplyOptimization(
             url,
             optimization_guide::proto::WALLETABLE_PASS_DETECTION_ALLOWLIST,
             /*optimization_metadata=*/nullptr) ==
         optimization_guide::OptimizationGuideDecision::kTrue;
}

void WalletablePassIngestionController::RegisterOptimizationTypes() {
  optimization_guide_decider_->RegisterOptimizationTypes(
      {optimization_guide::proto::WALLETABLE_PASS_DETECTION_ALLOWLIST});
}

}  // namespace wallet
