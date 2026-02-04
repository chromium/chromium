// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_immersive_overlay_view.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_immersive_web_view.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/fill_layout.h"

ReadAnythingImmersiveOverlayView::ReadAnythingImmersiveOverlayView(
    ContentsWebView* contents_web_view)
    : contents_web_view_(contents_web_view) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetID(VIEW_ID_READ_ANYTHING_OVERLAY);
  SetVisible(false);

  webcontents_attached_subscription_ =
      contents_web_view_->AddWebContentsAttachedCallback(base::BindRepeating(
          &ReadAnythingImmersiveOverlayView::OnWebContentsAttached,
          base::Unretained(this)));
  webcontents_detached_subscription_ =
      contents_web_view_->AddWebContentsDetachedCallback(base::BindRepeating(
          &ReadAnythingImmersiveOverlayView::OnWebContentsDetached,
          base::Unretained(this)));

  // If the contents_web_view already has a web contents, attach to it.
  if (contents_web_view_->web_contents()) {
    OnWebContentsAttached(contents_web_view_);
  }
}

ReadAnythingImmersiveOverlayView::~ReadAnythingImmersiveOverlayView() {
  UnsubscribeFromController();
}

void ReadAnythingImmersiveOverlayView::OnWebContentsAttached(
    views::WebView* web_view) {
  SubscribeToController(web_view);
}

void ReadAnythingImmersiveOverlayView::SubscribeToController(
    views::WebView* web_view) {
  content::WebContents* web_contents = web_view->GetWebContents();
  if (!web_contents) {
    return;
  }

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return;
  }

  controller_ = ReadAnythingController::From(tab);
  if (!controller_) {
    return;
  }
  controller_->AddImmersiveActivationObserver(this);
  controller_->AddObserver(this);
}

void ReadAnythingImmersiveOverlayView::OnWebContentsDetached(
    views::WebView* web_view) {
  UnsubscribeFromController();
}

void ReadAnythingImmersiveOverlayView::UnsubscribeFromController() {
  if (controller_) {
    controller_->RemoveImmersiveActivationObserver(this);
    controller_->RemoveObserver(this);
    controller_ = nullptr;
  }
}

void ReadAnythingImmersiveOverlayView::OnShowImmersive(
    ReadAnythingOpenTrigger trigger) {
  if (immersive_web_view_) {
    return;
  }

  if (controller_) {
    ShowUI(controller_->GetOrCreateWebUIWrapper(
               ReadAnythingController::PresentationState::kInImmersiveOverlay),
           trigger);
  }
}

void ReadAnythingImmersiveOverlayView::OnCloseImmersive() {
  std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>> wrapper =
      CloseUI();
  if (wrapper && controller_) {
    controller_->TransferWebUiOwnership(std::move(wrapper));
  }
}

void ReadAnythingImmersiveOverlayView::OnDestroyed() {
  UnsubscribeFromController();
}

void ReadAnythingImmersiveOverlayView::ShowUI(
    std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
        contents_wrapper,
    ReadAnythingOpenTrigger trigger) {
  CHECK(!immersive_web_view_);
  auto immersive_web_view = std::make_unique<ReadAnythingImmersiveWebView>(
      base::BindOnce(&ReadAnythingImmersiveOverlayView::OnShowUI,
                     base::Unretained(this)),
      std::move(contents_wrapper), trigger);
  immersive_web_view_ = AddChildView(std::move(immersive_web_view));
  immersive_view_focus_subscription_ =
      immersive_web_view_->AddWebContentsFocusedCallback(base::BindRepeating(
          &ReadAnythingImmersiveOverlayView::OnImmersiveWebViewFocused,
          base::Unretained(this)));
}

void ReadAnythingImmersiveOverlayView::OnShowUI() {
  SetVisible(true);

  // When IRM is being shown, we tell the renderer that the main webpage needs
  // to be treated as visible even though it's occluded, so it can generate
  // accessibility events we need for RM to function. We also set the underlying
  // web contents to be not accessible while IRM is open, so that it won't
  // receive screen reader focus or be navigable by keyboard.
  contents_web_view_->GetViewAccessibility().SetIsIgnored(true);
  contents_web_view_->SetFocusBehavior(views::View::FocusBehavior::NEVER);
}

std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
ReadAnythingImmersiveOverlayView::CloseUI() {
  immersive_view_focus_subscription_ = {};
  SetVisible(false);

  // We want the main web contents to be accessible again if IRM is closed and
  // the main webpage is now visible.
  contents_web_view_->GetViewAccessibility().SetIsIgnored(false);
  contents_web_view_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  CHECK(immersive_web_view_);
  std::unique_ptr<ReadAnythingImmersiveWebView> web_view =
      RemoveChildViewT(immersive_web_view_);
  immersive_web_view_ = nullptr;
  return web_view->CloseAndTakeContentsWrapper();
}

base::CallbackListSubscription
ReadAnythingImmersiveOverlayView::AddWebViewFocusedCallback(
    base::RepeatingCallback<void(views::WebView*)> callback) {
  return focus_callback_list_.Add(std::move(callback));
}

void ReadAnythingImmersiveOverlayView::OnImmersiveWebViewFocused(
    views::WebView* web_view) {
  focus_callback_list_.Notify(web_view);
}

BEGIN_METADATA(ReadAnythingImmersiveOverlayView)
END_METADATA
