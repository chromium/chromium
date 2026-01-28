// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_INGESTION_WALLETABLE_PASS_INGESTION_CONTROLLER_TEST_API_H_
#define COMPONENTS_WALLET_CORE_BROWSER_INGESTION_WALLETABLE_PASS_INGESTION_CONTROLLER_TEST_API_H_

#include <string>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "components/wallet/core/browser/ingestion/walletable_pass_ingestion_controller.h"
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

  std::optional<PassCategory> GetPassCategoryForURL(const GURL& url) {
    return controller_->GetPassCategoryForURL(url);
  }

  void ExtractWalletablePass(
      const GURL& url,
      PassCategory pass_category,
      const optimization_guide::proto::AnnotatedPageContent&
          annotated_page_content) {
    base::RepeatingClosure barrier = base::BindRepeating(
        &WalletablePassIngestionController::FinishExtraction,
        base::Unretained(&controller_.get()), url);
    controller_->ExtractWalletablePass(url, pass_category,
                                       annotated_page_content, barrier);
  }

  void StartWalletablePassDetectionFlow(const GURL& url) {
    controller_->StartWalletablePassDetectionFlow(url);
  }

  void ShowConsentBubble(const GURL& url, PassCategory pass_category) {
    controller_->ShowConsentBubble(url, pass_category);
  }

  void ShowSaveBubble(const GURL& url, WalletPass walletable_pass) {
    controller_->ShowSaveBubble(url, std::move(walletable_pass));
  }

  void MaybeStartExtraction(const GURL& url, PassCategory pass_category) {
    controller_->MaybeStartExtraction(url, pass_category);
  }

 private:
  const raw_ref<WalletablePassIngestionController> controller_;
};

inline WalletablePassIngestionControllerTestApi test_api(
    WalletablePassIngestionController* controller) {
  return WalletablePassIngestionControllerTestApi(controller);
}

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_INGESTION_WALLETABLE_PASS_INGESTION_CONTROLLER_TEST_API_H_
