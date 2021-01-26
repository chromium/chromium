// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_WORK_PROFILE_CONFIRMATION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_WORK_PROFILE_CONFIRMATION_UI_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/signin_web_dialog_ui.h"

class Browser;
class Profile;

namespace content {
class WebUIDataSource;
}

namespace ui {
class WebUI;
}

// WebUI controller for the work prorfile confirmation dialog.
//
// Note: This controller does not set the WebUI message handler. It is
// the responsibility of the caller to pass the correct message handler.
class WorkProfileConfirmationUI : public SigninWebDialogUI {
 public:
  explicit WorkProfileConfirmationUI(content::WebUI* web_ui);
  ~WorkProfileConfirmationUI() override;

  // SigninWebDialogUI:
  void InitializeMessageHandlerWithBrowser(Browser* browser) override;
  void InitializeMessageHandlerWithBrowser(
      Browser* browser,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback);

  // Initializes the message handler when there's no browser for `profile`
  // available (such as in the profile creation flow).
  void InitializeMessageHandlerWithProfile(
      Profile* profile,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback);

 private:
  // Adds a string resource with the given GRD |ids| to the WebUI data |source|
  // named as |name|. Also stores a reverse mapping from the localized version
  // of the string to the |ids| in order to later pass it to
  // SyncConfirmationHandler.
  void AddStringResource(content::WebUIDataSource* source,
                         const std::string& name,
                         int ids,
                         base::Optional<std::vector<base::string16>> params);

  DISALLOW_COPY_AND_ASSIGN(WorkProfileConfirmationUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_WORK_PROFILE_CONFIRMATION_UI_H_
