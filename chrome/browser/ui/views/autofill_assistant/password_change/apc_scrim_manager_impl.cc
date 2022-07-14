// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/apc_scrim_manager_impl.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "content/public/browser/web_contents.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace {

constexpr float kScrimOpacity = 0.5;

}  // namespace

std::unique_ptr<ApcScrimManager> ApcScrimManager::Create(
    content::WebContents* web_contents) {
  return std::make_unique<ApcScrimManagerImpl>(web_contents);
}

ApcScrimManagerImpl::ApcScrimManagerImpl(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  web_contents_ = web_contents;
  GetContentsWebView()->AddChildView(CreateOverlayView());
  observation_.Observe(GetContentsWebView());
}

ApcScrimManagerImpl::~ApcScrimManagerImpl() {
  std::unique_ptr<views::View> overlay_view =
      GetContentsWebView()->RemoveChildViewT(overlay_view_ref_);
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
  return BrowserView::GetBrowserViewForBrowser(
             chrome::FindBrowserWithWebContents(web_contents_))
      ->contents_web_view();
}

std::unique_ptr<views::View> ApcScrimManagerImpl::CreateOverlayView() {
  std::unique_ptr<views::View> view = std::make_unique<views::View>();
  view->SetVisible(false);
  view->SetBoundsRect(GetContentsWebView()->bounds());
  view->SetBackground(views::CreateSolidBackground(SK_ColorLTGRAY));
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
