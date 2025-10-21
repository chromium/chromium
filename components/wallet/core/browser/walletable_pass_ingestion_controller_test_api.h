// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_INGESTION_CONTROLLER_TEST_API_H_
#define COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_INGESTION_CONTROLLER_TEST_API_H_

#include <string>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/wallet/core/browser/walletable_pass_ingestion_controller.h"
#include "url/gurl.h"

namespace optimization_guide::proto {
class AnnotatedPageContent;
}

namespace wallet {

// Exposes some testing operations for WalletablePassIngestionController.
class WalletablePassIngestionControllerTestApi {
 public:
  explicit WalletablePassIngestionControllerTestApi(
      WalletablePassIngestionController* controller)
      : controller_(CHECK_DEREF(controller)) {}

  bool IsEligibleForExtraction(const GURL& url) {
    return controller_->IsEligibleForExtraction(url);
  }

  void ExtractWalletablePass(
      const GURL& url,
      const optimization_guide::proto::AnnotatedPageContent&
          annotated_page_content) {
    controller_->ExtractWalletablePass(url, annotated_page_content);
  }

  void StartWalletablePassDetectionFlow(const GURL& url) {
    controller_->StartWalletablePassDetectionFlow(url);
  }

  void ShowSaveBubble(std::unique_ptr<optimization_guide::proto::WalletablePass>
                          walletable_pass) {
    controller_->ShowSaveBubble(std::move(walletable_pass));
  }

 private:
  const raw_ref<WalletablePassIngestionController> controller_;
};

inline WalletablePassIngestionControllerTestApi test_api(
    WalletablePassIngestionController* controller) {
  return WalletablePassIngestionControllerTestApi(controller);
}

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_INGESTION_CONTROLLER_TEST_API_H_
