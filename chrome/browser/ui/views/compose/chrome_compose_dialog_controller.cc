// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/compose/chrome_compose_dialog_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// Default size from Figma spec. The size of the view will follow the requested
// size of the WebUI, once these are connected.
namespace {

const char kComposeURL[] = "chrome://compose/";

}

namespace chrome {

std::unique_ptr<compose::ComposeDialogController> ShowComposeDialog(
    content::WebContents& web_contents,
    const gfx::RectF& element_bounds_in_screen) {
  // The Compose dialog is not anchored to any particular View. Pass the
  // BrowserView so that it still knows about the Browser window, which is
  // needed to access the correct ColorProvider for theming.
  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(
      chrome::FindBrowserWithTab(&web_contents));
  auto controller =
      std::make_unique<ChromeComposeDialogController>(&web_contents);
  controller->ShowComposeDialog(anchor_view, element_bounds_in_screen);
  return controller;
}

}  // namespace chrome

ChromeComposeDialogController::~ChromeComposeDialogController() = default;

// TOOD(b/301371110): This function should accept an argument indicating whether
// it was called from the context menu. If so, open by popping in. Otherwise,
// open with an expanding animation.
void ChromeComposeDialogController::ShowComposeDialog(
    views::View* anchor_view,
    const gfx::RectF& element_bounds_in_screen) {
  if (!web_contents_) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  auto bubble_wrapper = std::make_unique<BubbleContentsWrapperT<ComposeUI>>(
      GURL(kComposeURL), profile, IDS_COMPOSE_DIALOG_TITLE);
  bubble_wrapper->ReloadWebContents();

  // This WebUI needs to know the calling BrowserContents so that the compose
  // request/result can be properly associated with the triggering form.
  bubble_wrapper->GetWebUIController()->set_triggering_web_contents(
      web_contents_.get());

  // The element will not be visible if it is outside the Browser View bounds,
  // so clamp the element bounds to be within them.
  gfx::Rect clamped_element_bounds =
      gfx::ToRoundedRect(element_bounds_in_screen);
  clamped_element_bounds.Intersect(anchor_view->GetBoundsInScreen());

  auto compose_dialog_view = std::make_unique<ComposeDialogView>(
      anchor_view, std::move(bubble_wrapper), clamped_element_bounds,
      views::BubbleBorder::Arrow::TOP_CENTER);
  bubble_ = compose_dialog_view->GetWeakPtr();
  views::BubbleDialogDelegateView::CreateBubble(std::move(compose_dialog_view));
  if (bubble_) {
    // This must be called after CreateBubble, as that resets the
    // |adjust_if_offscreen| field to the platform-dependent default.
    bubble_->set_adjust_if_offscreen(true);
  }
}

BubbleContentsWrapperT<ComposeUI>*
ChromeComposeDialogController::GetBubbleWrapper() const {
  if (bubble_) {
    return bubble_->bubble_wrapper();
  }
  return nullptr;
}

void ChromeComposeDialogController::ShowUI() {
  if (bubble_) {
    bubble_->ShowUI();
  }
}

// TODO(b/300939629): Flesh out implementation and cover other closing paths.
void ChromeComposeDialogController::Close() {
  auto* wrapper = GetBubbleWrapper();
  if (wrapper) {
    wrapper->CloseUI();
  }
}

ChromeComposeDialogController::ChromeComposeDialogController(
    content::WebContents* web_contents)
    : web_contents_(web_contents->GetWeakPtr()) {}
