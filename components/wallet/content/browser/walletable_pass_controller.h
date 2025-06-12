// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_CONTROLLER_H_
#define COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_CONTROLLER_H_

#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace wallet {

// It controls the detection of walletable passes on a web page.
//
// This controller is instantiated per-tab and manages its own lifetime by
// observing the WebContents it is attached to.
class WalletablePassController : public content::WebContentsObserver {
 public:
  explicit WalletablePassController(content::WebContents* web_contents);
  ~WalletablePassController() override;

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
};

}  // namespace wallet

#endif  // COMPONENTS_WALLET_CONTENT_BROWSER_WALLETABLE_PASS_CONTROLLER_H_
