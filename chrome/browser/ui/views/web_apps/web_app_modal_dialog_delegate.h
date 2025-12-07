// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_MODAL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_MODAL_DIALOG_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_observer.h"
#include "chrome/browser/picture_in_picture/scoped_picture_in_picture_occlusion_observation.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

namespace web_app {

// Base class for modal web app dialogs (install, launch, etc.)
// Covers common functionality for web app modal dialogs, such as closing on
// tab changes, closing on occlusion by the picture in picture dialog, and
// widget destruction.
class WebAppModalDialogDelegate : public ui::DialogModelDelegate,
                                  public content::WebContentsObserver,
                                  public PictureInPictureOcclusionObserver,
                                  public views::WidgetObserver {
 public:
  explicit WebAppModalDialogDelegate(content::WebContents* web_contents);
  ~WebAppModalDialogDelegate() override;

  // Once the dialog is shown, start tracking the widget for:
  // 1. Observing it to prevent picture in picture occlusion.
  // 2. Observing it for size changes so that it can be closed if needed.
  // 3. Tracking it as a security dialog so that extension popups do not appear
  // over it.
  void OnWidgetShownStartTracking(views::Widget* dialog_widget);

  // content::WebContentsObserver overrides:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(content::Page& page) override;

  // views::WidgetObserver override:
  void OnWidgetDestroyed(views::Widget* widget) override;

  // PictureInPictureOcclusionObserver overrides:
  void OnOcclusionStateChanged(bool occluded) override;

 protected:
  // Handle dialog close due to non-user actions - tab switch, navigation, etc.
  virtual void CloseDialogAsIgnored() = 0;

 private:
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  ScopedPictureInPictureOcclusionObservation occlusion_observation_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_MODAL_DIALOG_DELEGATE_H_
