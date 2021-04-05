// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_controller.h"
#include "chrome/browser/ui/media_router/media_router_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
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

MediaRouterDialogControllerViews::~MediaRouterDialogControllerViews() {
  Reset();
  media_router_ui_service_->RemoveObserver(this);
}

bool MediaRouterDialogControllerViews::ShowMediaRouterDialogForPresentation(
    std::unique_ptr<StartPresentationContext> context) {
  if (!GlobalMediaControlsCastStartStopEnabled()) {
    // Delegate to the base class, which will show the Cast dialog.
    return MediaRouterDialogController::ShowMediaRouterDialogForPresentation(
        std::move(context));
  }

  // Show global media controls instead of the Cast dialog.
  Browser* const browser = chrome::FindBrowserWithWebContents(initiator());
  BrowserView* const browser_view =
      browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
  ToolbarView* const toolbar_view =
      browser_view ? browser_view->toolbar() : nullptr;
  MediaToolbarButtonView* const media_button =
      toolbar_view ? toolbar_view->media_button() : nullptr;
  // Show the |media_button| before opening the dialog so that when the bubble
  // dialog is opened, it has an anchor.
  if (media_button) {
    media_button->media_toolbar_button_controller()->ShowToolbarButton();
    toolbar_view->Layout();
  }

  Profile* const profile =
      Profile::FromBrowserContext(initiator()->GetBrowserContext());
  MediaNotificationService* const service =
      MediaNotificationServiceFactory::GetForProfile(profile);
  service->OnStartPresentationContextCreated(std::move(context));
  // TODO(crbug/1111120): When |media_button| is null, we want to show the
  // global media controls anchored to the top of the web contents. As it is
  // now, it shows the dialog in the wrong place with a big blue border around
  // it.  Fixing the position probably involves doing something similar to the
  // computation of |anchor_bounds| in CreateMediaRouterDialog() below, but
  // just doing the same thing here doesn't work.  I suspect that approach
  // will work, though, once the issue causing the blue border is fixed.
  scoped_widget_observations_.AddObservation(
      MediaDialogView::ShowDialogForPresentationRequest(
          media_button, service, profile, initiator(),
          GlobalMediaControlsEntryPoint::kPresentation));
  return true;
}

void MediaRouterDialogControllerViews::CreateMediaRouterDialog(
    MediaRouterDialogOpenOrigin activation_location) {
  base::Time dialog_creation_time = base::Time::Now();
  if (GetActionController())
    GetActionController()->OnDialogShown();

  Profile* profile =
      Profile::FromBrowserContext(initiator()->GetBrowserContext());

  InitializeMediaRouterUI();
  Browser* browser = chrome::FindBrowserWithWebContents(initiator());
  if (browser) {
    // Show the Cast dialog anchored to the Cast toolbar button.
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    if (browser_view->toolbar()->cast_button()) {
      CastDialogView::ShowDialogWithToolbarAction(
          ui_.get(), browser, dialog_creation_time, activation_location);
    } else {
      CastDialogView::ShowDialogCenteredForBrowserWindow(
          ui_.get(), browser, dialog_creation_time, activation_location);
    }
  } else {
    // Show the Cast dialog anchored to the top of the web contents.
    gfx::Rect anchor_bounds = initiator()->GetContainerBounds();
    // Set the height to 0 so that the dialog gets anchored to the top of the
    // window.
    anchor_bounds.set_height(0);
    CastDialogView::ShowDialogCentered(anchor_bounds, ui_.get(), profile,
                                       dialog_creation_time,
                                       activation_location);
  }
  scoped_widget_observations_.AddObservation(
      CastDialogView::GetCurrentDialogWidget());

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
  DCHECK(scoped_widget_observations_.IsObservingSource(widget));
  if (ui_)
    ui_->LogMediaSinkStatus();
  Reset();
  scoped_widget_observations_.RemoveObservation(widget);
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
  ui_ = std::make_unique<MediaRouterUI>(initiator());
  if (start_presentation_context_) {
    ui_->InitWithStartPresentationContextAndMirroring(
        std::move(start_presentation_context_));
  } else {
    ui_->InitWithDefaultMediaSourceAndMirroring();
  }
}

MediaRouterActionController*
MediaRouterDialogControllerViews::GetActionController() {
  return media_router_ui_service_->action_controller();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaRouterDialogControllerViews)

}  // namespace media_router
