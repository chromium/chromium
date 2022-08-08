// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/apc_scrim_manager_impl.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "content/public/browser/web_contents.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace {

constexpr float kScrimOpacity = 0.26;

}  // namespace

std::unique_ptr<ApcScrimManager> ApcScrimManager::Create(
    content::WebContents* web_contents) {
  return std::make_unique<ApcScrimManagerImpl>(web_contents);
}

ApcScrimManagerImpl::ApcScrimManagerImpl(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  browser_ = chrome::FindBrowserWithWebContents(web_contents);
  GetContentsWebView()->AddChildView(CreateOverlayView());
  observation_.Observe(GetContentsWebView());
}

ApcScrimManagerImpl::~ApcScrimManagerImpl() {
  // Makes sure the browser is still in the browser list.
  // If yes, we can safely access it. The browser might not be in the list
  // in the case where a tab is dragged to a browser B causing
  // browser A (containing this view) to be deleted.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser_ == browser) {
      BrowserView* browser_view =
          BrowserView::GetBrowserViewForBrowser(browser);
      // If the browser view no longer exists, neither its children do.
      if (!browser_view)
        return;
      if (browser_view->contents_web_view()->Contains(overlay_view_ref_)) {
        browser_view->contents_web_view()->RemoveChildViewT(overlay_view_ref_);
      }
      return;
    }
  }
}

void ApcScrimManagerImpl::Show() {
  overlay_view_ref_->SetVisible(true);
}

void ApcScrimManagerImpl::Hide() {
  overlay_view_ref_->SetVisible(false);
}

bool ApcScrimManagerImpl::GetVisible() {
  return overlay_view_ref_->GetVisible();
}

raw_ptr<views::View> ApcScrimManagerImpl::GetContentsWebView() {
  DCHECK(browser_);
  DCHECK(BrowserView::GetBrowserViewForBrowser(browser_));
  return BrowserView::GetBrowserViewForBrowser(browser_)->contents_web_view();
}

std::unique_ptr<views::View> ApcScrimManagerImpl::CreateOverlayView() {
  std::unique_ptr<views::View> view = std::make_unique<views::View>();
  view->SetVisible(false);
  view->SetBoundsRect(GetContentsWebView()->bounds());
  view->SetBackground(views::CreateSolidBackground(SK_ColorBLACK));
  view->SetPaintToLayer();
  view->layer()->SetName("PasswordChangeRunScrim");
  view->layer()->SetOpacity(kScrimOpacity);

  overlay_view_ref_ = view.get();
  return view;
}

void ApcScrimManagerImpl::OnViewBoundsChanged(views::View* observed_view) {
  overlay_view_ref_->SetBoundsRect(observed_view->bounds());
}

void ApcScrimManagerImpl::OnVisibilityChanged(content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    Hide();
  } else if (visibility == content::Visibility::VISIBLE) {
    Show();
  }
}
