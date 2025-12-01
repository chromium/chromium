// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WALLET_CHROME_WALLETABLE_PASS_CLIENT_H_
#define CHROME_BROWSER_WALLET_CHROME_WALLETABLE_PASS_CLIENT_H_

#include "base/memory/raw_ref.h"
#include "components/wallet/content/browser/content_walletable_pass_ingestion_controller.h"
#include "components/wallet/core/browser/data_models/walletable_pass.h"
#include "components/wallet/core/browser/walletable_pass_client.h"

namespace optimization_guide {
class OptimizationGuideDecider;
class RemoteModelExecutor;
}  // namespace optimization_guide

namespace strike_database {
class StrikeDatabaseBase;
}  // namespace strike_database
namespace tabs {
class TabInterface;
}  // namespace tabs

class PrefService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace wallet {

class ContentWalletablePassIngestionController;
class WalletablePassConsentBubbleController;
class WalletablePassSaveBubbleController;

// The Chrome implementation of `wallet::WalletablePassClient`.
//
// This class bridges the core wallet component with browser services, such as
// the Optimization Guide and UI interactions (e.g., showing a saving pass
// bubble). Its lifecycle is scoped to a single tab and managed by
// `TabFeatures`.
class ChromeWalletablePassClient : public WalletablePassClient {
 public:
  explicit ChromeWalletablePassClient(tabs::TabInterface* tab);

  ~ChromeWalletablePassClient() override;

  // WalleablePassClient implementation.
  optimization_guide::OptimizationGuideDecider* GetOptimizationGuideDecider()
      override;

  optimization_guide::RemoteModelExecutor* GetRemoteModelExecutor() override;

  strike_database::StrikeDatabaseBase* GetStrikeDatabase() override;
  PrefService* GetPrefService() override;
  signin::IdentityManager* GetIdentityManager() override;
  GeoIpCountryCode GetGeoIpCountryCode() override;

  void ShowWalletablePassConsentBubble(
      optimization_guide::proto::PassCategory pass_category,
      WalletablePassBubbleResultCallback callback) override;
  void ShowWalletablePassSaveBubble(
      WalletablePass pass,
      WalletablePassBubbleResultCallback callback) override;

 private:
  const raw_ref<tabs::TabInterface> tab_;

  ContentWalletablePassIngestionController controller_;
  std::unique_ptr<WalletablePassConsentBubbleController>
      consent_bubble_controller_;
  std::unique_ptr<WalletablePassSaveBubbleController> save_bubble_controller_;
};

}  // namespace wallet

#endif  // CHROME_BROWSER_WALLET_CHROME_WALLETABLE_PASS_CLIENT_H_
