// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_interstitial_delegate.h"

#include <utility>

const content::InterstitialPageDelegate::TypeID
    LoginInterstitialDelegate::kTypeForTesting =
        &LoginInterstitialDelegate::kTypeForTesting;

LoginInterstitialDelegate::LoginInterstitialDelegate(
    content::WebContents* web_contents,
    const GURL& request_url,
    base::OnceClosure callback)
    : callback_(std::move(callback)),
      interstitial_page_(content::InterstitialPage::Create(web_contents,
                                                           true,
                                                           request_url,
                                                           this)) {
  // When shown, interstitial page takes ownership of |this|.
  interstitial_page_->Show();
}

LoginInterstitialDelegate::~LoginInterstitialDelegate() {
}

void LoginInterstitialDelegate::Proceed() {
  interstitial_page_->Proceed();
}

void LoginInterstitialDelegate::DontProceed() {
  interstitial_page_->DontProceed();
}

base::WeakPtr<LoginInterstitialDelegate>
LoginInterstitialDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void LoginInterstitialDelegate::CommandReceived(const std::string& command) {
  std::move(callback_).Run();
}

content::InterstitialPageDelegate::TypeID
LoginInterstitialDelegate::GetTypeForTesting() {
  return LoginInterstitialDelegate::kTypeForTesting;
}

std::string LoginInterstitialDelegate::GetHTMLContents() {
  // Showing an interstitial results in a new navigation, and a new navigation
  // closes all modal dialogs on the page. Therefore the login prompt must be
  // shown after the interstitial is displayed. This is done by sending a
  // command from the interstitial page as soon as it is loaded.
  return std::string(
      "<!DOCTYPE html>"
      "<html><body><script>"
      "window.domAutomationController.send('1');"
      "</script></body></html>");
}
