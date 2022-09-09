// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"

#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_view.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace qrcode_generator {

QRCodeGeneratorBubbleController::~QRCodeGeneratorBubbleController() {
  HideBubble();
}

// static
bool QRCodeGeneratorBubbleController::IsGeneratorAvailable(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS())
    return false;

  return true;
}

// static
QRCodeGeneratorBubbleController* QRCodeGeneratorBubbleController::Get(
    content::WebContents* web_contents) {
  QRCodeGeneratorBubbleController::CreateForWebContents(web_contents);
  QRCodeGeneratorBubbleController* controller =
      QRCodeGeneratorBubbleController::FromWebContents(web_contents);
  return controller;
}

void QRCodeGeneratorBubbleController::ShowBubble(const GURL& url,
                                                 bool show_back_button) {
  // Ignore subsequent calls to open the dialog if it already is open.
  if (bubble_shown_)
    return;

  Browser* browser = chrome::FindBrowserWithWebContents(&GetWebContents());
  if (!browser || !browser->window())
    return;

  bubble_shown_ = true;
  qrcode_generator_bubble_ = browser->window()->ShowQRCodeGeneratorBubble(
      &GetWebContents(), url, show_back_button);

  UpdateIcon();
}

void QRCodeGeneratorBubbleController::HideBubble() {
  if (qrcode_generator_bubble_) {
    qrcode_generator_bubble_->Hide();
    qrcode_generator_bubble_ = nullptr;
  }
}

QRCodeGeneratorBubbleView*
QRCodeGeneratorBubbleController::qrcode_generator_bubble_view() const {
  return qrcode_generator_bubble_;
}

base::OnceClosure QRCodeGeneratorBubbleController::GetOnBubbleClosedCallback() {
  return base::BindOnce(&QRCodeGeneratorBubbleController::OnBubbleClosed,
                        weak_ptr_factory_.GetWeakPtr());
}

base::OnceClosure
QRCodeGeneratorBubbleController::GetOnBackButtonPressedCallback() {
  return base::BindOnce(&QRCodeGeneratorBubbleController::OnBackButtonPressed,
                        weak_ptr_factory_.GetWeakPtr());
}

void QRCodeGeneratorBubbleController::OnBubbleClosed() {
  bubble_shown_ = false;
  qrcode_generator_bubble_ = nullptr;
}

void QRCodeGeneratorBubbleController::OnBackButtonPressed() {
  sharing_hub::SharingHubBubbleController* controller =
      sharing_hub::SharingHubBubbleController::CreateOrGetFromWebContents(
          &GetWebContents());
  controller->ShowBubble(share::ShareAttempt(&GetWebContents()));
}

void QRCodeGeneratorBubbleController::UpdateIcon() {
  Browser* browser = chrome::FindBrowserWithWebContents(&GetWebContents());
  // UpdateIcon() can be called during browser teardown.
  if (!browser)
    return;

  if (sharing_hub::SharingHubOmniboxEnabled(
          GetWebContents().GetBrowserContext())) {
    browser->window()->UpdatePageActionIcon(PageActionIconType::kSharingHub);
  } else {
    browser->window()->UpdatePageActionIcon(
        PageActionIconType::kQRCodeGenerator);
  }
}

QRCodeGeneratorBubbleController::QRCodeGeneratorBubbleController(
    content::WebContents* web_contents)
    : content::WebContentsUserData<QRCodeGeneratorBubbleController>(
          *web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(QRCodeGeneratorBubbleController);

}  // namespace qrcode_generator
