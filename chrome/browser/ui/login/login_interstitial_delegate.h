// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_LOGIN_INTERSTITIAL_DELEGATE_H_
#define CHROME_BROWSER_UI_LOGIN_LOGIN_INTERSTITIAL_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// Placeholder interstitial for HTTP login prompts. This interstitial makes the
// omnibox show the correct url when the login prompt is visible.
class LoginInterstitialDelegate : public content::InterstitialPageDelegate {
 public:
  // Interstitial type, used in tests.
  static const content::InterstitialPageDelegate::TypeID kTypeForTesting;

  LoginInterstitialDelegate(content::WebContents* web_contents,
                            const GURL& request_url,
                            base::OnceClosure callback);

  ~LoginInterstitialDelegate() override;

  void Proceed();
  void DontProceed();
  base::WeakPtr<LoginInterstitialDelegate> GetWeakPtr();

  // content::InterstitialPageDelegate:
  void CommandReceived(const std::string& command) override;
  content::InterstitialPageDelegate::TypeID GetTypeForTesting() override;

 protected:
  std::string GetHTMLContents() override;

 private:
  base::OnceClosure callback_;
  content::InterstitialPage* interstitial_page_;
  base::WeakPtrFactory<LoginInterstitialDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginInterstitialDelegate);
};

#endif  // CHROME_BROWSER_UI_LOGIN_LOGIN_INTERSTITIAL_DELEGATE_H_
