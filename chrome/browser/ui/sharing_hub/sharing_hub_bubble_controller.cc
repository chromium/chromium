// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace sharing_hub {

SharingHubBubbleController::~SharingHubBubbleController() {
  if (sharing_hub_bubble_view_) {
    sharing_hub_bubble_view_->Hide();
  }
}

// static
SharingHubBubbleController*
SharingHubBubbleController::CreateOrGetFromWebContents(
    content::WebContents* web_contents) {
  SharingHubBubbleController::CreateForWebContents(web_contents);
  SharingHubBubbleController* controller =
      SharingHubBubbleController::FromWebContents(web_contents);
  return controller;
}

void SharingHubBubbleController::HideBubble() {
  if (sharing_hub_bubble_view_) {
    sharing_hub_bubble_view_->Hide();
    sharing_hub_bubble_view_ = nullptr;
  }
}

void SharingHubBubbleController::ShowBubble() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  sharing_hub_bubble_view_ =
      browser->window()->ShowSharingHubBubble(web_contents_, this, true);
}

SharingHubBubbleView* SharingHubBubbleController::sharing_hub_bubble_view()
    const {
  return sharing_hub_bubble_view_;
}

std::u16string SharingHubBubbleController::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_SHARING_HUB_TITLE);
}

Profile* SharingHubBubbleController::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

bool SharingHubBubbleController::ShouldOfferOmniboxIcon() {
  if (!web_contents_)
    return false;

  // TODO(1186845): Check enterprise policy

  return true;
}

void SharingHubBubbleController::OnBubbleClosed() {
  sharing_hub_bubble_view_ = nullptr;
}

SharingHubBubbleController::SharingHubBubbleController() = default;

SharingHubBubbleController::SharingHubBubbleController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SharingHubBubbleController)

}  // namespace sharing_hub
