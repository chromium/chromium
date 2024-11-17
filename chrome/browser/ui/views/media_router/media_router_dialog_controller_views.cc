// Copyright 2018 The Chromium Authors
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
#include "chrome/browser/ui/views/media_router/cast_dialog_coordinator.h"
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
  if (!GlobalMediaControlsCastStartStopEnabled(
          initiator()->GetBrowserContext())) {
    // Delegate to the base class, which will show the Cast dialog.
    return MediaRouterDialogController::ShowMediaRouterDialogForPresentation(
        std::move(context));
  }
#if BUILDFLAG(IS_CHROMEOS)
  ShowGlobalMediaControlsDialog(std::move(context));
#else
  ShowGlobalMediaControlsDialogAsync(std::move(context));
#endif  // BUILDFLAG(IS_CHROMEOS)
  return true;
}

void MediaRouterDialogControllerViews::CreateMediaRouterDialog(
    MediaRouterDialogActivationLocation activation_location) {
  base::Time dialog_creation_time = base::Time::Now();
  if (GetActionController()) {
    GetActionController()->OnDialogShown();
  }
  Profile* profile =
      Profile::FromBrowserContext(initiator()->GetBrowserContext());

  InitializeMediaRouterUI();
  Browser* browser = chrome::FindBrowserWithTab(initiator());
  BrowserView* browser_view =
      browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
  if (browser_view) {
    // Show the Cast dialog anchored to the Cast toolbar button.
    if (browser_view->toolbar()->GetCastButton()) {
      cast_dialog_coordinator_.ShowDialogWithToolbarAction(
          ui_.get(), browser, dialog_creation_time, activation_location);
    } else {
      cast_dialog_coordinator_.ShowDialogCenteredForBrowserWindow(
          ui_.get(), browser, dialog_creation_time, activation_location);
    }
  } else {
    // Show the Cast dialog anchored to the top of the web contents.
    gfx::Rect anchor_bounds = initiator()->GetContainerBounds();
    // Set the height to 0 so that the dialog gets anchored to the top of the
    // window.
    anchor_bounds.set_height(0);
    cast_dialog_coordinator_.ShowDialogCentered(anchor_bounds, ui_.get(),
                                                profile, dialog_creation_time,
                                                activation_location);
  }
  scoped_widget_observations_.AddObservation(
      cast_dialog_coordinator_.GetCastDialogWidget());

  if (dialog_creation_callback_) {
    dialog_creation_callback_.Run();
  }
  MediaRouterMetrics::RecordMediaRouterDialogActivationLocation(
      activation_location);
}

void MediaRouterDialogControllerViews::CloseMediaRouterDialog() {
  if (IsShowingMediaRouterDialog()) {
    cast_dialog_coordinator_.Hide();
  }
}

bool MediaRouterDialogControllerViews::IsShowingMediaRouterDialog() const {
  return cast_dialog_coordinator_.IsShowing();
}

void MediaRouterDialogControllerViews::Reset() {
  // If |ui_| is null, Reset() has already been called.
  if (ui_) {
    if (GetActionController()) {
      GetActionController()->OnDialogHidden();
    }
    ui_.reset();
    MediaRouterDialogController::Reset();
  }
}

void MediaRouterDialogControllerViews::OnWidgetDestroying(
    views::Widget* widget) {
  DCHECK(scoped_widget_observations_.IsObservingSource(widget));
  if (ui_) {
    ui_->LogMediaSinkStatus();
  }
  Reset();
  scoped_widget_observations_.RemoveObservation(widget);
}

void MediaRouterDialogControllerViews::SetDialogCreationCallbackForTesting(
    base::RepeatingClosure callback) {
  dialog_creation_callback_ = std::move(callback);
}

void MediaRouterDialogControllerViews::SetHideMediaButtonForTesting(bool hide) {
  hide_media_button_for_testing_ = hide;
}

MediaRouterDialogControllerViews::MediaRouterDialogControllerViews(
    WebContents* web_contents)
    : content::WebContentsUserData<MediaRouterDialogControllerViews>(
          *web_contents),
      MediaRouterDialogController(web_contents),
      media_router_ui_service_(GetMediaRouterUIService(web_contents)) {
  DCHECK(media_router_ui_service_);
  media_router_ui_service_->AddObserver(this);
}

void MediaRouterDialogControllerViews::OnServiceDisabled() {
  CloseMediaRouterDialog();
  Reset();
}

void MediaRouterDialogControllerViews::InitializeMediaRouterUI() {
  ui_ = start_presentation_context_
            ? MediaRouterUI::CreateWithStartPresentationContextAndMirroring(
                  initiator(), std::move(start_presentation_context_))
            : MediaRouterUI::CreateWithDefaultMediaSourceAndMirroring(
                  initiator());
  ui_->RegisterDestructor(
      base::BindOnce(&MediaRouterDialogControllerViews::DestroyMediaRouterUI,
                     // Safe to use base::Unretained here: the callback being
                     // bound is held by the MediaRouterUI we are creating and
                     // owning, and ownership of |ui_| is never transferred
                     // away from this object.
                     base::Unretained(this)));
}

void MediaRouterDialogControllerViews::DestroyMediaRouterUI() {
  ui_.reset();
}

#if BUILDFLAG(IS_CHROMEOS)
void MediaRouterDialogControllerViews::ShowGlobalMediaControlsDialog(
    std::unique_ptr<StartPresentationContext> context) {
  Profile* const profile =
      Profile::FromBrowserContext(initiator()->GetBrowserContext());
  MediaNotificationServiceFactory::GetForProfile(profile)->ShowDialogAsh(
      std::move(context));
}
#else
void MediaRouterDialogControllerViews::ShowGlobalMediaControlsDialogAsync(
    std::unique_ptr<StartPresentationContext> context) {
  // Show the WebContents requesting a dialog.
  initiator()->GetDelegate()->ActivateContents(initiator());

  Profile* const profile =
      Profile::FromBrowserContext(initiator()->GetBrowserContext());
  MediaNotificationService* const service =
      MediaNotificationServiceFactory::GetForProfile(profile);
  service->OnStartPresentationContextCreated(std::move(context));
  // This needs to be async because it needs to happen after UI preparations
  // (done through OnStartPresentationContextCreated()) that may happen
  // asynchronously as it crosses a Mojo boundary.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaRouterDialogControllerViews::ShowGlobalMediaControlsDialog,
          weak_ptr_factory_.GetWeakPtr()));
}

void MediaRouterDialogControllerViews::ShowGlobalMediaControlsDialog() {
  Profile* const profile =
      Profile::FromBrowserContext(initiator()->GetBrowserContext());
  MediaNotificationService* const service =
      MediaNotificationServiceFactory::GetForProfile(profile);
  MediaToolbarButtonView* const media_button = GetMediaButton();
  // If there exists a media button, anchor the dialog to this media button.
  if (media_button) {
    scoped_widget_observations_.AddObservation(MediaDialogView::ShowDialog(
        media_button, views::BubbleBorder::TOP_RIGHT, service, profile,
        initiator(),
        global_media_controls::GlobalMediaControlsEntryPoint::kPresentation));
    return;
  }
  Browser* const browser = chrome::FindBrowserWithTab(initiator());
  BrowserView* const browser_view =
      browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
  // If there exists a browser_view, anchor the dialog to the top center of the
  // browser_view. This is necessary only for Mac, but works for other
  // platforms.
  if (browser_view) {
    scoped_widget_observations_.AddObservation(MediaDialogView::ShowDialog(
        browser_view->top_container(), views::BubbleBorder::TOP_CENTER, service,
        profile, initiator(),
        global_media_controls::GlobalMediaControlsEntryPoint::kPresentation));
  } else {
    // Show the GMC dialog anchored to the top of the web contents.
    gfx::Rect anchor_bounds = initiator()->GetContainerBounds();
    anchor_bounds.set_height(0);
    scoped_widget_observations_.AddObservation(
        MediaDialogView::ShowDialogCentered(
            anchor_bounds, service, profile, initiator(),
            global_media_controls::GlobalMediaControlsEntryPoint::
                kPresentation));
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

MediaToolbarButtonView* MediaRouterDialogControllerViews::GetMediaButton() {
  if (hide_media_button_for_testing_) {
    return nullptr;
  }
  Browser* const browser = chrome::FindBrowserWithTab(initiator());
  BrowserView* const browser_view =
      browser ? BrowserView::GetBrowserViewForBrowser(browser) : nullptr;
  ToolbarView* const toolbar_view =
      browser_view ? browser_view->toolbar() : nullptr;
  MediaToolbarButtonView* media_button =
      toolbar_view ? toolbar_view->media_button() : nullptr;

  if (!media_button) {
    return nullptr;
  }
  // Show the |media_button| before opening the dialog so that when the bubble
  // dialog is opened, it has an anchor.
  media_button->media_toolbar_button_controller()->ShowToolbarButton();
  toolbar_view->DeprecatedLayoutImmediately();

  return media_button;
}

CastToolbarButtonController*
MediaRouterDialogControllerViews::GetActionController() {
  return media_router_ui_service_->action_controller();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaRouterDialogControllerViews);

}  // namespace media_router
