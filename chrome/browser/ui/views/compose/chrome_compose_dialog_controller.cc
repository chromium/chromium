// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/compose/chrome_compose_dialog_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_metrics.h"
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
    compose::LogOpenComposeDialogResult(
        compose::OpenComposeDialogResult::kNoWebContents);
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
    compose::LogOpenComposeDialogResult(
        compose::OpenComposeDialogResult::kSuccess);
    // This must be called after CreateBubble, as that resets the
    // |adjust_if_offscreen| field to the platform-dependent default.
    bubble_->set_adjust_if_offscreen(true);

    if (base::FeatureList::IsEnabled(
            compose::features::kEnableComposeSavedStateNotification)) {
      // Prevent closing when losing focus to show saved state notification.
      bubble_->set_close_on_deactivate(false);

      // Observe parent widget for resize and repositioning events.
      if (bubble_->GetWidget() && bubble_->GetWidget()->parent()) {
        widget_observation_.Observe(bubble_->GetWidget()->parent());
      }

      zoom_observation_.Observe(
          zoom::ZoomController::FromWebContents(web_contents_.get()));
    }
  } else {
    compose::LogOpenComposeDialogResult(
        compose::OpenComposeDialogResult::kFailedCreatingComposeDialogView);
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
  // This will no-op if there is no observation.
  widget_observation_.Reset();
  zoom_observation_.Reset();
  auto* wrapper = GetBubbleWrapper();
  if (wrapper) {
    wrapper->CloseUI();
  }
}

bool ChromeComposeDialogController::IsDialogShowing() {
  return bubble_ && !bubble_->GetWidget()->IsClosed();
}

void ChromeComposeDialogController::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  if (base::FeatureList::IsEnabled(
          compose::features::kEnableComposeSavedStateNotification) &&
      IsDialogShowing() && widget == bubble_->GetWidget()->parent()) {
    // Resizing or repositioning the parent view should close the compose
    // dialog since it does not yet follow the associated HTML element.
    Close();
  }
}

void ChromeComposeDialogController::OnWidgetDestroying(views::Widget* widget) {
  // This will no-op if there is no observation.
  widget_observation_.Reset();
}

void ChromeComposeDialogController::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  if (base::FeatureList::IsEnabled(
          compose::features::kEnableComposeSavedStateNotification) &&
      IsDialogShowing()) {
    // Zooming should close the compose dialog since it does not yet change
    // position to follow the associated HTML element.
    Close();
  }
}

void ChromeComposeDialogController::OnZoomControllerDestroyed(
    zoom::ZoomController* zoom_controller) {
  // This will no-op if there is no observation.
  zoom_observation_.Reset();
}

ChromeComposeDialogController::ChromeComposeDialogController(
    content::WebContents* web_contents)
    : web_contents_(web_contents->GetWeakPtr()) {}
