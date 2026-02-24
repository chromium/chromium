// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sad_tab_controller.h"

#include "chrome/browser/ui/views/sad_tab_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

SadTabController::SadTabController(content::WebContents* sad_tab_web_contents,
                                   SadTabKind sad_tab_kind)
    : SadTab(sad_tab_web_contents, sad_tab_kind) {
  view_ = std::make_unique<SadTabView>(
      this, web_contents(), kind(), GetTitle(), GetInfoMessage(),
      GetSubMessages(), GetErrorCodeFormatString(), GetCrashedErrorCode(),
      GetButtonTitle(), GetHelpLinkTitle());
}

SadTabController::~SadTabController() = default;

void SadTabController::ReinstallInWebView() {
  view_->ReinstallInWebView();
}

void SadTabController::SetBackgroundRadii(const gfx::RoundedCornersF& radii) {
  view_->SetBackgroundRadii(radii);
}

gfx::RoundedCornersF SadTabController::GetBackgroundRadii() const {
  return view_->GetBackgroundRadii();
}

void SadTabController::RequestFocus() {
  view_->RequestFocus();
}

// SadTab::Create implementation moved here.
std::unique_ptr<SadTab> SadTab::Create(content::WebContents* web_contents,
                                       SadTabKind kind) {
  return std::make_unique<SadTabController>(web_contents, kind);
}
