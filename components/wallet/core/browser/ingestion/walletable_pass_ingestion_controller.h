// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CORE_BROWSER_INGESTION_WALLETABLE_PASS_INGESTION_CONTROLLER_H_
#define COMPONENTS_WALLET_CORE_BROWSER_INGESTION_WALLETABLE_PASS_INGESTION_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/walletable_pass_extraction.pb.h"
#include "components/wallet/core/browser/data_models/wallet_barcode.h"
#include "components/wallet/core/browser/data_models/wallet_pass.h"
#include "components/wallet/core/browser/ingestion/walletable_pass_client.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "components/wallet/core/browser/strike_databases/walletable_pass_consent_strike_database.h"
#include "components/wallet/core/browser/strike_databases/walletable_pass_save_strike_database_by_host.h"

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

  // Callback to be invoked once the barcode detection is complete.
  using BarcodeDetectionCallback =
      base::OnceCallback<void(std::vector<WalletBarcode>)>;

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

  // Searches the Optimization Guide's allowlists to find a pass category
  // for the given `url`.
  //
  // Each PassCategory has its own allowlist. This method returns the
  // *first* category that lists the `url`.
  //
  // Returns the matching PassCategory if found, or std::nullopt if the `url`
  // is not in any pass allowlist.
  std::optional<PassCategory> GetPassCategoryForURL(const GURL& url) const;

  // Gets the title of current page.
  virtual std::string GetPageTitle() const = 0;

  // Gets the annotated page content for the current page. `callback` is
  // invoked upon completion.
  virtual void GetAnnotatedPageContent(
      AnnotatedPageContentCallback callback) = 0;

  // Detects barcodes on the current page. `callback` is invoked upon
  // completion.
  virtual void DetectBarcodes(BarcodeDetectionCallback callback) = 0;

  // Shows the "Consent" bubble to the user, allowing them to agree to use the
  // feature.
  void ShowConsentBubble(const GURL& url, PassCategory pass_category);

  // Shows the "Save" bubble to the user, allowing them to save the provided
  // pass.
  void ShowSaveBubble(const GURL& url, WalletPass walletable_pass);

 private:
  friend class WalletablePassIngestionControllerTestApi;

  struct ProcessingState {
    ProcessingState();
    ~ProcessingState();
    ProcessingState(const ProcessingState&);
    ProcessingState& operator=(const ProcessingState&);

    std::vector<WalletBarcode> detected_barcodes;
    std::optional<WalletPass> extracted_pass;
  };

  // Extracts a walletable pass from the provided page content. This method
  // invokes the Optimization Guide's model executor to perform the extraction.
  void ExtractWalletablePass(
      const GURL& url,
      PassCategory pass_category,
      optimization_guide::proto::AnnotatedPageContent annotated_page_content,
      base::RepeatingClosure barrier);

  void MaybeStartExtraction(const GURL& url, PassCategory pass_category);

  // Callback for when the annotated page content is available.
  void OnGetAnnotatedPageContent(
      const GURL& url,
      PassCategory pass_category,
      base::RepeatingClosure barrier,
      std::optional<optimization_guide::proto::AnnotatedPageContent>
          annotated_page_content);

  // Callback for when the barcode detection is complete.
  void OnBarcodesDetected(base::RepeatingClosure barrier,
                          std::vector<WalletBarcode> barcodes);

  // Callback for when the barcode detection is complete for boarding passes.
  void OnBoardingPassBarcodesDetected(const GURL& url,
                                      std::vector<WalletBarcode> barcodes);

  // Callback for when the pass extraction from the model executor is complete.
  void OnExtractWalletablePass(
      PassCategory pass_category,
      base::RepeatingClosure barrier,
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  // Called once all parallel tasks (barcode detection and LLM extraction)
  // are complete. It merges the detected barcode (if any) into the extracted
  // pass and then shows the save bubble if a pass was successfully extracted.
  void FinishExtraction(const GURL& url);

  // Callback invoked when the user interacts with the consent bubble (e.g.,
  // accepts, declines, or dismisses).
  void OnGetConsentBubbleResult(
      const GURL& url,
      PassCategory pass_category,
      WalletablePassClient::WalletablePassBubbleResult result);

  // Callback invoked when the user interacts with the save bubble (e.g.,
  // accepts, declines, or dismisses).
  void OnGetSaveBubbleResult(
      const GURL& url,
      WalletPass walletable_pass,
      WalletablePassClient::WalletablePassBubbleResult result);

  // Callback invoked when the pass is saved successfully or fails.
  void OnPassSaved(
      const GURL& url,
      const base::expected<WalletPass, WalletHttpClient::WalletRequestError>&
          result);

  // A raw reference to the client, which owns `this` and therefore outlives
  // it.
  const raw_ref<WalletablePassClient> client_;

  std::unique_ptr<WalletablePassSaveStrikeDatabaseByHost> save_strike_db_;

  std::unique_ptr<WalletablePassConsentStrikeDatabase> consent_strike_db_;

  ProcessingState processing_state_;

  base::WeakPtrFactory<WalletablePassIngestionController> weak_ptr_factory_{
      this};

  // Weak pointer factory used for individual extraction requests. Invalidated
  // at the start of each new extraction to cancel stale callbacks.
  base::WeakPtrFactory<WalletablePassIngestionController>
      processing_weak_ptr_factory_{this};
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CORE_BROWSER_INGESTION_WALLETABLE_PASS_INGESTION_CONTROLLER_H_
