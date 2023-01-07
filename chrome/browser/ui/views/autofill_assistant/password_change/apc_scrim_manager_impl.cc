// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/apc_scrim_manager_impl.h"

#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/ax_mode.h"
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
    : content::WebContentsObserver(web_contents),
      restore_accessibility_mode_timer_(
          std::make_unique<base::OneShotTimer>()) {
  browser_ = chrome::FindBrowserWithWebContents(web_contents);
  GetContentsWebView()->AddChildView(CreateOverlayView());
  observation_.Observe(GetContentsWebView());
}

ApcScrimManagerImpl::~ApcScrimManagerImpl() {
  restore_accessibility_mode_timer_->Stop();
  // In the case where the webcontents has not been destroyed before this class.
  if (web_contents())
    RestartOriginalAccessibilityMode(/*focus_on_web_contents=*/false);

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
  if (is_disabled_)
    return;

  restore_accessibility_mode_timer_->Stop();
  overlay_view_ref_->SetVisible(true);
  web_contents()->SetAccessibilityMode(ui::AXMode::kNone);
}

void ApcScrimManagerImpl::Hide() {
  overlay_view_ref_->SetVisible(false);

  restore_accessibility_mode_timer_->Stop();
  restore_accessibility_mode_timer_->Start(
      FROM_HERE, base::Seconds(2),
      base::BindOnce(&ApcScrimManagerImpl::RestartOriginalAccessibilityMode,
                     base::Unretained(this), /*focus_on_web_contents=*/true));
}

void ApcScrimManagerImpl::ShutDown() {
  is_disabled_ = true;
  overlay_view_ref_->SetVisible(false);
  restore_accessibility_mode_timer_->Stop();
  RestartOriginalAccessibilityMode(/*focus_on_web_contents=*/false);
}

void ApcScrimManagerImpl::SetIsDisabled(bool is_disabled) {
  is_disabled_ = is_disabled;
}

bool ApcScrimManagerImpl::GetIsDisabled() const {
  return is_disabled_;
}

bool ApcScrimManagerImpl::GetVisible() const {
  return overlay_view_ref_->GetVisible() &&
         web_contents()->GetAccessibilityMode() == ui::AXMode::kNone;
}

void ApcScrimManagerImpl::RestartOriginalAccessibilityMode(
    bool focus_on_web_contents) {
  web_contents()->SetAccessibilityMode(
      content::BrowserAccessibilityState::GetInstance()
          ->GetAccessibilityMode());
  if (focus_on_web_contents)
    FocusOnWebContents();
}

void ApcScrimManagerImpl::FocusOnWebContents() {
  web_contents()->Focus();
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
    scrim_visible_on_webcontents_hide_ = GetVisible();
    // We hide the scrim and immediately reset the accessibility mode.
    overlay_view_ref_->SetVisible(false);
    RestartOriginalAccessibilityMode(/*focus_on_web_contents=*/false);
  } else if (visibility == content::Visibility::VISIBLE &&
             scrim_visible_on_webcontents_hide_) {
    Show();
  }
}

void ApcScrimManagerImpl::SetRestoreAccessibilityModeTimerForTest(
    std::unique_ptr<base::OneShotTimer> restore_accessibility_mode_timer) {
  restore_accessibility_mode_timer_ =
      std::move(restore_accessibility_mode_timer);
}
