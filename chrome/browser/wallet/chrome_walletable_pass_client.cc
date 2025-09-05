// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/wallet/chrome_walletable_pass_client.h"

#include "base/check_deref.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/tabs/public/tab_interface.h"
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

optimization_guide::OptimizationGuideModelExecutor*
ChromeWalletablePassClient::GetOptimizationGuideModelExecutor() {
  Profile* profile =
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());
  return OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
}

}  // namespace wallet
