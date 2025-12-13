// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CONTENT_BROWSER_CONTENT_WALLETABLE_PASS_INGESTION_CONTROLLER_H_
#define COMPONENTS_WALLET_CONTENT_BROWSER_CONTENT_WALLETABLE_PASS_INGESTION_CONTROLLER_H_

#include "components/wallet/core/browser/walletable_pass_ingestion_controller.h"
#include "content/public/browser/web_contents_observer.h"

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
      public content::WebContentsObserver {
 public:
  ContentWalletablePassIngestionController(content::WebContents* web_contents,
                                           WalletablePassClient* client);

  ContentWalletablePassIngestionController(
      const ContentWalletablePassIngestionController&) = delete;
  ContentWalletablePassIngestionController& operator=(
      const ContentWalletablePassIngestionController&) = delete;

  ~ContentWalletablePassIngestionController() override;

  // WalletablePassIngestionController:
  std::string GetPageTitle() const override;

  void GetAnnotatedPageContent(AnnotatedPageContentCallback callback) override;

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CONTENT_BROWSER_CONTENT_WALLETABLE_PASS_INGESTION_CONTROLLER_H_
