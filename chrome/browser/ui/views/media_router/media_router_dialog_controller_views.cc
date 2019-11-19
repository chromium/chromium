// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace media_router {

namespace {

MediaRouterUIService* GetMediaRouterUIService(WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return MediaRouterUIService::Get(profile);
}

}  // namespace

// static
MediaRouterDialogController*
MediaRouterDialogController::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // This call does nothing if the controller already exists.
  MediaRouterDialogControllerViews::CreateForWebContents(web_contents);
  return MediaRouterDialogControllerViews::FromWebContents(web_contents);
}

MediaRouterDialogControllerViews::~MediaRouterDialogControllerViews() {
  Reset();
  media_router_ui_service_->RemoveObserver(this);
}

void MediaRouterDialogControllerViews::CreateMediaRouterDialog() {
  base::Time dialog_creation_time = base::Time::Now();
  if (GetActionController())
    GetActionController()->OnDialogShown();

  Profile* profile =
      Profile::FromBrowserContext(initiator()->GetBrowserContext());
  InitializeMediaRouterUI();

  Browser* browser = chrome::FindBrowserWithWebContents(initiator());
  if (browser) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    if (browser_view->toolbar()->cast_button()) {
      CastDialogView::ShowDialogWithToolbarAction(ui_.get(), browser,
                                                  dialog_creation_time);
    } else {
      CastDialogView::ShowDialogCenteredForBrowserWindow(ui_.get(), browser,
                                                         dialog_creation_time);
    }
  } else {
    gfx::Rect anchor_bounds = initiator()->GetContainerBounds();
    // Set the height to 0 so that the dialog gets anchored to the top of the
    // window.
    anchor_bounds.set_height(0);
    CastDialogView::ShowDialogCentered(anchor_bounds, ui_.get(), profile,
                                       dialog_creation_time);
  }
  scoped_widget_observer_.Add(CastDialogView::GetCurrentDialogWidget());
  if (dialog_creation_callback_)
    dialog_creation_callback_.Run();
}

void MediaRouterDialogControllerViews::CloseMediaRouterDialog() {
  CastDialogView::HideDialog();
}

bool MediaRouterDialogControllerViews::IsShowingMediaRouterDialog() const {
  return CastDialogView::IsShowing();
}

void MediaRouterDialogControllerViews::Reset() {
  // If |ui_| is null, Reset() has already been called.
  if (ui_) {
    if (IsShowingMediaRouterDialog() && GetActionController())
      GetActionController()->OnDialogHidden();
    ui_.reset();
    MediaRouterDialogController::Reset();
  }
}

void MediaRouterDialogControllerViews::OnWidgetClosing(views::Widget* widget) {
  DCHECK(scoped_widget_observer_.IsObserving(widget));
  Reset();
  scoped_widget_observer_.Remove(widget);
}

void MediaRouterDialogControllerViews::SetDialogCreationCallbackForTesting(
    base::RepeatingClosure callback) {
  dialog_creation_callback_ = std::move(callback);
}

MediaRouterDialogControllerViews::MediaRouterDialogControllerViews(
    WebContents* web_contents)
    : MediaRouterDialogController(web_contents),
      media_router_ui_service_(GetMediaRouterUIService(web_contents)) {
  DCHECK(media_router_ui_service_);
  media_router_ui_service_->AddObserver(this);
}

void MediaRouterDialogControllerViews::OnServiceDisabled() {
  CloseMediaRouterDialog();
  Reset();
}

void MediaRouterDialogControllerViews::InitializeMediaRouterUI() {
  ui_ = std::make_unique<MediaRouterViewsUI>();
  PresentationServiceDelegateImpl* delegate =
      PresentationServiceDelegateImpl::FromWebContents(initiator());
  if (!start_presentation_context_) {
    ui_->InitWithDefaultMediaSource(initiator(), delegate);
  } else {
    ui_->InitWithStartPresentationContext(
        initiator(), delegate, std::move(start_presentation_context_));
  }
}

MediaRouterActionController*
MediaRouterDialogControllerViews::GetActionController() {
  return media_router_ui_service_->action_controller();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaRouterDialogControllerViews)

}  // namespace media_router
