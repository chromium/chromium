// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_WORK_PROFILE_CONFIRMATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_WORK_PROFILE_CONFIRMATION_HANDLER_H_

#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace signin {
class IdentityManager;
}

// WebUI message handler for the work profile confirmation dialog.
// IdentityManager calls in this class use signin::ConsentLevel::kNotRequired
// because the user hasn't consented to anything yet.
class WorkProfileConfirmationHandler : public content::WebUIMessageHandler,
                                       public signin::IdentityManager::Observer,
                                       public BrowserListObserver {
 public:
  // Creates a WorkProfileConfirmationHandler for the |profile|. All strings in
  // the corresponding Web UI should be represented in |string_to_grd_id_map|
  // and mapped to their GRD IDs. If |browser| is provided, its signin view
  // controller will be notified of the rendered size of the web page.
  WorkProfileConfirmationHandler(
      Profile* profile,
      Browser* browser,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback);
  ~WorkProfileConfirmationHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

 protected:
  // Handles "confirm" message from the page. No arguments.
  // This message is sent when the user confirms that they want complete sign in
  // with a work profile.
  virtual void HandleConfirm(const base::ListValue* args);

  // Handles "cancel" message from the page. No arguments.
  // This message is sent when the user clicks "cancel" on the work profile
  // confirmation dialog, which aborts signin and prevents the creation of a
  // work profile.
  virtual void HandleCancel(const base::ListValue* args);

  // Handles the web ui message sent when the html content is done being laid
  // out and it's time to resize the native view hosting it to fit. |args| is
  // a single integer value for the height the native view should resize to.
  virtual void HandleInitializedWithSize(const base::ListValue* args);

  // Handles the "accountImageRequest" message sent after the
  // "account-image-changed" WebUIListener was added. This method calls
  // |SetUserImageURL| with the signed-in user's picture url.
  virtual void HandleAccountImageRequest(const base::ListValue* args);

  // Sets the profile picture shown in the dialog to the image at |url|.
  virtual void SetUserImageURL(const std::string& url);

  // TODO: Update comment
  // Closes the modal signin window and calls
  // DiceTurnSyncOnHelper::SigninChoice with |result|. |result|
  // indicates the option chosen by the user in the confirmation UI.
  void CloseModalSigninWindow(DiceTurnSyncOnHelper::SigninChoice result);

 private:
  Profile* profile_;

  // Records whether the user clicked on Undo, Ok, or Settings.
  bool did_user_explicitly_interact_ = false;

  // Weak reference to the browser that showed the work profile confirmation
  // dialog (if such a dialog exists).
  Browser* browser_;

  signin::IdentityManager* identity_manager_;

  DiceTurnSyncOnHelper::SigninChoiceCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(WorkProfileConfirmationHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_WORK_PROFILE_CONFIRMATION_HANDLER_H_
