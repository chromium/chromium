// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHORTCUTS_CREATE_DESKTOP_SHORTCUT_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_SHORTCUTS_CREATE_DESKTOP_SHORTCUT_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_observer.h"
#include "chrome/browser/picture_in_picture/scoped_picture_in_picture_occlusion_observation.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "chrome/browser/ui/views/shortcuts/create_desktop_shortcut.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/dialog_model.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

class Profile;

namespace shortcuts {

// Appends the user name of the profile to old_title in parenthesis if there is
// more than 1 profile on the device.
std::u16string AppendProfileNameToTitleIfNeeded(Profile* profile,
                                                std::u16string old_title);

class CreateDesktopShortcutDelegate : public ui::DialogModelDelegate,
                                      public content::WebContentsObserver,
                                      public PictureInPictureOcclusionObserver {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCreateShortcutDialogOkButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCreateShortcutDialogTitleFieldId);

  CreateDesktopShortcutDelegate(content::WebContents* web_contents,
                                CreateShortcutDialogCallback final_callback);

  ~CreateDesktopShortcutDelegate() override;

  // Starts observing the create desktop shortcut dialog's widget for picture in
  // picture occlusion if any.
  void StartObservingForPictureInPictureOcclusion(views::Widget* dialog_widget);

  void OnAccept();
  void OnClose();
  void OnTitleUpdated(const std::u16string& trimmed_text_field_data);

  base::WeakPtr<CreateDesktopShortcutDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(content::Page& page) override;

  // PictureInPictureOcclusionObserver overrides:
  void OnOcclusionStateChanged(bool occluded) override;

 private:
  void CloseDialogAsIgnored();

  CreateShortcutDialogCallback final_callback_;
  std::u16string text_field_data_;
  ScopedPictureInPictureOcclusionObservation occlusion_observation_{this};
  base::WeakPtrFactory<CreateDesktopShortcutDelegate> weak_ptr_factory_{this};
};

}  // namespace shortcuts

#endif  // CHROME_BROWSER_UI_VIEWS_SHORTCUTS_CREATE_DESKTOP_SHORTCUT_DELEGATE_H_
