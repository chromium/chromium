// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/content/browser/walletable_pass_controller.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace wallet {

WalletablePassController::WalletablePassController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WalletablePassController::~WalletablePassController() = default;

void WalletablePassController::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  // TODO(crbug.com/422366321): Add walletable pass detection logic here.
}

}  // namespace wallet
