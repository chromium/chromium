// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_observer.h"
#include "chrome/browser/picture_in_picture/scoped_picture_in_picture_occlusion_observation.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class PrefService;

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace feature_engagement {
class Tracker;
}

namespace webapps {
class MlInstallOperationTracker;
}  // namespace webapps

namespace views {
class Widget;
}  // namespace views

namespace web_app {

enum InstallDialogType { kSimple, kDetailed, kDiy, kMaxValue = kDiy };

inline constexpr int kIconSize = 32;

// When pre-populating the name field (using the web app title) we
// should try to make some effort to not suggest things we know work extra
// poorly when used as filenames in the OS. This is especially problematic when
// creating apps for pages that have no title, because then the URL of the
// page will be used as a suggestion, and (if accepted by the user) the shortcut
// name will look really weird. For example, MacOS will convert a colon (:) to a
// forward-slash (/), and Windows will convert the colons to spaces. MacOS even
// goes a step further and collapses multiple consecutive forward-slashes in
// localized names into a single forward-slash. That means, using 'https://foo'
// as an example, an app with a display name of 'https/foo' is created on
// MacOS and 'https   foo' on Windows. By stripping away the schema, we will be
// greatly reducing the frequency of apps having weird names. Note: This
// does not affect user's ability to use URLs as an app name (which would
// result in a weird filename), it only restricts what we suggest as titles.
std::u16string NormalizeSuggestedAppTitle(const std::u16string& title);

class WebAppInstallDialogDelegate : public ui::DialogModelDelegate,
                                    public content::WebContentsObserver,
                                    public PictureInPictureOcclusionObserver {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDiyAppsDialogOkButtonId);

  WebAppInstallDialogDelegate(
      content::WebContents* web_contents,
      std::unique_ptr<WebAppInstallInfo> install_info,
      std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker,
      AppInstallationAcceptanceCallback callback,
      PwaInProductHelpState iph_state,
      PrefService* prefs,
      feature_engagement::Tracker* tracker,
      InstallDialogType dialog_type);

  ~WebAppInstallDialogDelegate() override;

  // Starts observing the install dialog's widget for picture in picture
  // occlusion if any.
  void StartObservingForPictureInPictureOcclusion(
      views::Widget* install_dialog_widget);

  void OnAccept();
  void OnCancel();
  void OnClose();

  // This is called when the dialog has been either accepted, cancelled, closed
  // or destroyed without an user-action.
  void OnDestroyed();

  // Takes care of enabling or disabling the dialog model's OK button for DIY
  // apps based on changes in the text field, and also keeps track of the text
  // field's contents.
  void OnTextFieldChangedMaybeUpdateButton(
      const std::u16string& text_field_contents);

  base::WeakPtr<WebAppInstallDialogDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // content::WebContentsObserver overrides:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(content::Page& page) override;

  // PictureInPictureOcclusionObserver overrides:
  void OnOcclusionStateChanged(bool occluded) override;

 private:
  void CloseDialogAsIgnored();
  void MeasureIphOnDialogClose();
  void MeasureAcceptUserActionsForInstallDialog();
  void MeasureCancelUserActionsForInstallDialog();

  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<WebAppInstallInfo> install_info_;
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker_;
  AppInstallationAcceptanceCallback callback_;
  PwaInProductHelpState iph_state_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<feature_engagement::Tracker> tracker_;
  InstallDialogType dialog_type_;
  std::u16string text_field_contents_;
  bool received_user_response_ = false;
  ScopedPictureInPictureOcclusionObservation occlusion_observation_{this};

  base::WeakPtrFactory<WebAppInstallDialogDelegate> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_INSTALL_DIALOG_DELEGATE_H_
