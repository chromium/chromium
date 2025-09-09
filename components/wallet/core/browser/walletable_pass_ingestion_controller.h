// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_INGESTION_CONTROLLER_H_
#define COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_INGESTION_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/wallet/core/browser/walletable_pass_client.h"

class GURL;

namespace optimization_guide {
class ModelQualityLogEntry;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace wallet {

// Controls the detection of walletable passes on a web page.
class WalletablePassIngestionController {
 public:
  // Callback to be invoked once the annotated page content is available.
  using AnnotatedPageContentCallback = base::OnceCallback<void(
      std::optional<optimization_guide::proto::AnnotatedPageContent>)>;

  explicit WalletablePassIngestionController(WalletablePassClient* client);

  virtual ~WalletablePassIngestionController();

  // Not copyable or movable.
  WalletablePassIngestionController(const WalletablePassIngestionController&) =
      delete;
  WalletablePassIngestionController& operator=(
      const WalletablePassIngestionController&) = delete;

  // Starts the walletable pass detection flow for the given URL.
  void StartWalletablePassDetectionFlow(const GURL& url);

 protected:
  // Registers optimization types with the Optimization Guide to query the pass
  // extraction allowlist.
  void RegisterOptimizationTypes();

  // Checks if the URL is eligible for pass extraction. This is determined by
  // consulting an allowlist managed by the Optimization Guide.
  bool IsEligibleForExtraction(const GURL& url) const;

  // Gets the annotated page content for the current page. `callback` is
  // invoked upon completion.
  virtual void GetAnnotatedPageContent(
      AnnotatedPageContentCallback callback) = 0;

  // Extracts a walletable pass from the provided page content. This method
  // invokes the Optimization Guide's model executor to perform the extraction.
  void ExtractWalletablePass(
      const std::string& url,
      optimization_guide::proto::AnnotatedPageContent annotated_page_content);

 private:
  friend class WalletablePassIngestionControllerTestApi;

  // Callback for when the annotated page content is available.
  void OnGetAnnotatedPageContent(
      const GURL& url,
      std::optional<optimization_guide::proto::AnnotatedPageContent>
          annotated_page_content);

  // Callback for when the pass extraction from the model executor is complete.
  void OnExtractWalletablePass(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // A raw reference to the client, which owns `this` and therefore outlives
  // it.
  const raw_ref<WalletablePassClient> client_;

  base::WeakPtrFactory<WalletablePassIngestionController> weak_ptr_factory_{
      this};
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_WALLETABLE_PASS_INGESTION_CONTROLLER_H_
