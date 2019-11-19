// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_VIEWS_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/views/media_router/media_router_views_ui.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace media_router {

// A Views implementation of MediaRouterDialogController.
class MediaRouterDialogControllerViews
    : public content::WebContentsUserData<MediaRouterDialogControllerViews>,
      public MediaRouterDialogController,
      public views::WidgetObserver,
      public MediaRouterUIService::Observer {
 public:
  ~MediaRouterDialogControllerViews() override;

  // MediaRouterDialogController:
  void CreateMediaRouterDialog() override;
  void CloseMediaRouterDialog() override;
  bool IsShowingMediaRouterDialog() const override;
  void Reset() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  // Sets a callback to be called whenever a dialog is created.
  void SetDialogCreationCallbackForTesting(base::RepeatingClosure callback);

 private:
  friend class content::WebContentsUserData<MediaRouterDialogControllerViews>;
  friend class MediaRouterUiForTest;

  // Use MediaRouterDialogController::GetOrCreateForWebContents() to create
  // an instance.
  explicit MediaRouterDialogControllerViews(content::WebContents* web_contents);

  // MediaRouterUIService::Observer:
  void OnServiceDisabled() override;

  // Initializes |ui_|.
  void InitializeMediaRouterUI();

  // MediaRouterActionController is responsible for showing and hiding the
  // toolbar action. It's owned by MediaRouterUIService and it may be nullptr.
  MediaRouterActionController* GetActionController();

  MediaRouterViewsUI* ui() { return ui_.get(); }

  // Responsible for notifying the dialog view of dialog model updates and
  // sending route requests to MediaRouter. Set to nullptr when the dialog is
  // closed.
  std::unique_ptr<MediaRouterViewsUI> ui_;

  base::RepeatingClosure dialog_creation_callback_;

  ScopedObserver<views::Widget, views::WidgetObserver> scoped_widget_observer_{
      this};

  // Service that provides MediaRouterActionController. It outlives |this|.
  MediaRouterUIService* const media_router_ui_service_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogControllerViews);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_VIEWS_H_
