// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_coordinator.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class MediaToolbarButtonView;

namespace media_router {

class MediaRouterUI;
class StartPresentationContext;

// A Views implementation of MediaRouterDialogController.
class MediaRouterDialogControllerViews
    : public content::WebContentsUserData<MediaRouterDialogControllerViews>,
      public MediaRouterDialogController,
      public views::WidgetObserver,
      public MediaRouterUIService::Observer {
 public:
  MediaRouterDialogControllerViews(const MediaRouterDialogControllerViews&) =
      delete;
  MediaRouterDialogControllerViews& operator=(
      const MediaRouterDialogControllerViews&) = delete;

  ~MediaRouterDialogControllerViews() override;

  // MediaRouterDialogController:
  bool ShowMediaRouterDialogForPresentation(
      std::unique_ptr<StartPresentationContext> context) override;
  void CreateMediaRouterDialog(
      MediaRouterDialogActivationLocation activation_location) override;
  void CloseMediaRouterDialog() override;
  bool IsShowingMediaRouterDialog() const override;
  void Reset() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Sets a callback to be called whenever a dialog is created.
  void SetDialogCreationCallbackForTesting(base::RepeatingClosure callback);

  void SetHideMediaButtonForTesting(bool hide);

  CastDialogCoordinator& GetCastDialogCoordinatorForTesting() {
    return cast_dialog_coordinator_;
  }

 private:
  friend class content::WebContentsUserData<MediaRouterDialogControllerViews>;
  friend class MediaRouterCastUiForTest;

  // Use MediaRouterDialogController::GetOrCreateForWebContents() to create
  // an instance.
  explicit MediaRouterDialogControllerViews(content::WebContents* web_contents);

  // MediaRouterUIService::Observer:
  void OnServiceDisabled() override;

  // Initializes and destroys |ui_| respectively.
  void InitializeMediaRouterUI();
  void DestroyMediaRouterUI();

#if BUILDFLAG(IS_CHROMEOS)
  // Show the GMC dialog in the Ash UI.
  void ShowGlobalMediaControlsDialog(
      std::unique_ptr<StartPresentationContext> context);
#else
  // If there exists a media button, show the GMC dialog anchored to the media
  // button. Otherwise, show the dialog anchored to the top center of the web
  // contents.
  void ShowGlobalMediaControlsDialogAsync(
      std::unique_ptr<StartPresentationContext> context);
  void ShowGlobalMediaControlsDialog();
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Returns the media button from the browser that initiates the request to
  // open the dialog. Returns nullptr if:
  // (1) the browser does not have a media button (i.e. the browser is
  // running a PWA.) or (2) |hide_media_button_for_testing_| is true.
  MediaToolbarButtonView* GetMediaButton();

  // CastToolbarButtonController is responsible for showing and hiding the
  // toolbar action. It's owned by MediaRouterUIService and it may be nullptr.
  CastToolbarButtonController* GetActionController();

  MediaRouterUI* ui() { return ui_.get(); }

  // Responsible for notifying the dialog view of dialog model updates and
  // sending route requests to MediaRouter. Set to nullptr when the dialog is
  // closed. Not used for presentation requests when
  // GlobalMediaControlsCastStartStopEnabled() returns true.
  std::unique_ptr<MediaRouterUI> ui_;

  CastDialogCoordinator cast_dialog_coordinator_;

  base::RepeatingClosure dialog_creation_callback_;

  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      scoped_widget_observations_{this};

  // Service that provides CastToolbarButtonController. It outlives |this|.
  const raw_ptr<MediaRouterUIService> media_router_ui_service_;

  bool hide_media_button_for_testing_ = false;

  base::WeakPtrFactory<MediaRouterDialogControllerViews> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_VIEWS_H_
