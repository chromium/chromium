// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_PROTOCOL_HANDLER_INTENT_PICKER_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_PROTOCOL_HANDLER_INTENT_PICKER_DIALOG_VIEW_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/web_apps/launch_app_user_choice_dialog_view.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

// User choice dialog for PWA protocol handling:
// https://web.dev/url-protocol-handler/
class WebAppProtocolHandlerIntentPickerView
    : public LaunchAppUserChoiceDialogView {
 public:
  METADATA_HEADER(WebAppProtocolHandlerIntentPickerView);

  WebAppProtocolHandlerIntentPickerView(
      GURL url,
      Profile* profile,
      const AppId& app_id,
      chrome::WebAppLaunchAcceptanceCallback close_callback);

  WebAppProtocolHandlerIntentPickerView(
      const WebAppProtocolHandlerIntentPickerView&) = delete;
  WebAppProtocolHandlerIntentPickerView& operator=(
      const WebAppProtocolHandlerIntentPickerView&) = delete;
  ~WebAppProtocolHandlerIntentPickerView() override;

 protected:
  std::unique_ptr<views::View> CreateAboveAppInfoView() override;
  std::unique_ptr<views::View> CreateBelowAppInfoView() override;
  std::u16string GetRememberChoiceString() override;

 private:
  const GURL url_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_PROTOCOL_HANDLER_INTENT_PICKER_DIALOG_VIEW_H_
