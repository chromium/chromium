// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/compose/chrome_compose_dialog_controller.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/compose/core/browser/compose_client.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/compose/core/browser/config.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace chrome {

std::unique_ptr<compose::ComposeDialogController> ShowComposeDialog(
    content::WebContents& web_contents,
    const gfx::RectF& element_bounds_in_screen,
    compose::ComposeClient::FieldIdentifier field_ids) {
  // The Compose dialog is not anchored to any particular View. Pass the
  // BrowserView so that it still knows about the Browser window, which is
  // needed to access the correct ColorProvider for theming.
  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(
      chrome::FindBrowserWithTab(&web_contents));
  auto controller =
      std::make_unique<ChromeComposeDialogController>(&web_contents, field_ids);
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
  auto bubble_wrapper =
      std::make_unique<WebUIContentsWrapperT<ComposeUntrustedUI>>(
          GURL(chrome::kChromeUIUntrustedComposeUrl), profile,
          IDS_COMPOSE_DIALOG_TITLE);

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
      if (bubble_->GetWidget()) {
        widget_observation_.Observe(bubble_->GetWidget());
      }
    }
  } else {
    compose::LogOpenComposeDialogResult(
        compose::OpenComposeDialogResult::kFailedCreatingComposeDialogView);
  }
}

WebUIContentsWrapperT<ComposeUntrustedUI>*
ChromeComposeDialogController::GetBubbleWrapper() const {
  if (bubble_) {
    return bubble_->bubble_wrapper();
  }
  return nullptr;
}

void ChromeComposeDialogController::ShowUI(
    base::OnceClosure focus_lost_callback) {
  focus_lost_callback_ = std::move(focus_lost_callback);
  if (bubble_) {
    bubble_->ShowUI();
  }
}

// TODO(b/300939629): Flesh out implementation and cover other closing paths.
void ChromeComposeDialogController::Close() {
  // This will no-op if there is no observation.
  widget_observation_.Reset();
  focus_lost_callback_.Reset();
  auto* wrapper = GetBubbleWrapper();
  if (wrapper) {
    wrapper->CloseUI();
  }
}

bool ChromeComposeDialogController::IsDialogShowing() {
  return bubble_ && !bubble_->GetWidget()->IsClosed();
}

void ChromeComposeDialogController::OnWidgetDestroying(views::Widget* widget) {
  if (focus_lost_callback_) {
    const compose::Config& config = compose::GetComposeConfig();
    // TODO(b/328730979): Add slight delay so that focus lost callback can be
    // called after all focus-related events have been processed.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ChromeComposeDialogController::OnAfterWidgetDestroyed,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Milliseconds(config.focus_lost_delay_milliseconds));
  }
  // This will no-op if there is no observation.
  widget_observation_.Reset();
}

const compose::ComposeClient::FieldIdentifier&
ChromeComposeDialogController::GetFieldIds() {
  return field_ids_;
}

void ChromeComposeDialogController::OnAfterWidgetDestroyed() {
  if (focus_lost_callback_) {
    std::move(focus_lost_callback_).Run();
  }
}

ChromeComposeDialogController::ChromeComposeDialogController(
    content::WebContents* web_contents,
    compose::ComposeClient::FieldIdentifier field_ids)
    : web_contents_(web_contents->GetWeakPtr()), field_ids_(field_ids) {}
