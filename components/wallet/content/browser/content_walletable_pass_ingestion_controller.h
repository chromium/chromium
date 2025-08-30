// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CONTENT_BROWSER_CONTENT_WALLETABLE_PASS_INGESTION_CONTROLLER_H_
#define COMPONENTS_WALLET_CONTENT_BROWSER_CONTENT_WALLETABLE_PASS_INGESTION_CONTROLLER_H_

#include "components/wallet/core/browser/walletable_pass_ingestion_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace wallet {

// It controls the detection of walletable passes on a web page.
//
// This controller is instantiated per-tab and manages its own lifetime by
// observing the `WebContents` it is attached to.
class ContentWalletablePassIngestionController
    : public WalletablePassIngestionController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<
          ContentWalletablePassIngestionController> {
 public:
  ContentWalletablePassIngestionController(
      const ContentWalletablePassIngestionController&) = delete;
  ContentWalletablePassIngestionController& operator=(
      const ContentWalletablePassIngestionController&) = delete;

  ~ContentWalletablePassIngestionController() override;

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  friend class content::WebContentsUserData<
      ContentWalletablePassIngestionController>;
  ContentWalletablePassIngestionController(
      content::WebContents* web_contents,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CONTENT_BROWSER_CONTENT_WALLETABLE_PASS_INGESTION_CONTROLLER_H_
