// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/wallet/chrome_walletable_pass_client.h"

#include "base/check_deref.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/wallet/walletable_pass_consent_bubble_controller.h"
#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_controller.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strike_database/strike_database.h"
#include "components/tabs/public/tab_interface.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/web_contents.h"

namespace wallet {

ChromeWalletablePassClient::ChromeWalletablePassClient(tabs::TabInterface* tab)
    : tab_(CHECK_DEREF(tab)), controller_(tab_->GetContents(), this) {}

ChromeWalletablePassClient::~ChromeWalletablePassClient() = default;

optimization_guide::OptimizationGuideDecider*
ChromeWalletablePassClient::GetOptimizationGuideDecider() {
  Profile* profile =
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());
  return OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
}

optimization_guide::RemoteModelExecutor*
ChromeWalletablePassClient::GetRemoteModelExecutor() {
  Profile* profile =
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());
  return OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
}

strike_database::StrikeDatabaseBase*
ChromeWalletablePassClient::GetStrikeDatabase() {
  Profile* profile =
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());
  return autofill::StrikeDatabaseFactory::GetForProfile(profile);
}

PrefService* ChromeWalletablePassClient::GetPrefService() {
  Profile* profile =
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());
  return profile->GetPrefs();
}

signin::IdentityManager* ChromeWalletablePassClient::GetIdentityManager() {
  Profile* profile =
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());
  return IdentityManagerFactory::GetForProfile(profile);
}

GeoIpCountryCode ChromeWalletablePassClient::GetGeoIpCountryCode() {
  if (!g_browser_process || !g_browser_process->variations_service()) {
    return GeoIpCountryCode();
  }

  return GeoIpCountryCode(base::ToUpperASCII(
      g_browser_process->variations_service()->GetStoredPermanentCountry()));
}

void ChromeWalletablePassClient::ShowWalletablePassConsentBubble(
    optimization_guide::proto::PassCategory pass_category,
    WalletablePassBubbleResultCallback callback) {
  if (!consent_bubble_controller_) {
    consent_bubble_controller_ =
        std::make_unique<WalletablePassConsentBubbleController>(&tab_.get());
  }
  consent_bubble_controller_->SetUpAndShowConsentBubble(pass_category,
                                                        std::move(callback));
}

void ChromeWalletablePassClient::ShowWalletablePassSaveBubble(
    WalletablePass pass,
    WalletablePassBubbleResultCallback callback) {
  if (!save_bubble_controller_) {
    save_bubble_controller_ =
        std::make_unique<WalletablePassSaveBubbleController>(&tab_.get());
  }
  save_bubble_controller_->SetUpAndShowSaveBubble(std::move(pass),
                                                  std::move(callback));
}

}  // namespace wallet
