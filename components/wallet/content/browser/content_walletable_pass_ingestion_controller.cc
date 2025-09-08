// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/content/browser/content_walletable_pass_ingestion_controller.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace wallet {

ContentWalletablePassIngestionController::
    ContentWalletablePassIngestionController(content::WebContents* web_contents,
                                             WalletablePassClient* client)
    : WalletablePassIngestionController(client),
      content::WebContentsObserver(web_contents) {}

ContentWalletablePassIngestionController::
    ~ContentWalletablePassIngestionController() = default;

void ContentWalletablePassIngestionController::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->IsInPrimaryMainFrame() ||
      !IsEligibleForExtraction(validated_url)) {
    return;
  }
  // TODO(crbug.com/422366321): Add walletable pass detection logic here.
}

}  // namespace wallet
