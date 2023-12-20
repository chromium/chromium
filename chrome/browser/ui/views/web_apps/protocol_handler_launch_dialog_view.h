// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_LAUNCH_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_LAUNCH_DIALOG_VIEW_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/web_apps/launch_app_user_choice_dialog_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

// User choice dialog for PWA protocol handling, shown before launching a PWA to
// handle a protocol launch. See https://web.dev/url-protocol-handler/
class ProtocolHandlerLaunchDialogView : public LaunchAppUserChoiceDialogView {
  METADATA_HEADER(ProtocolHandlerLaunchDialogView,
                  LaunchAppUserChoiceDialogView)

 public:
  ProtocolHandlerLaunchDialogView(
      GURL url,
      Profile* profile,
      const webapps::AppId& app_id,
      WebAppLaunchAcceptanceCallback close_callback);

  ProtocolHandlerLaunchDialogView(const ProtocolHandlerLaunchDialogView&) =
      delete;
  ProtocolHandlerLaunchDialogView& operator=(
      const ProtocolHandlerLaunchDialogView&) = delete;
  ~ProtocolHandlerLaunchDialogView() override;

 protected:
  std::unique_ptr<views::View> CreateAboveAppInfoView() override;
  std::unique_ptr<views::View> CreateBelowAppInfoView() override;
  std::u16string GetRememberChoiceString() override;

 private:
  const GURL url_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_PROTOCOL_HANDLER_LAUNCH_DIALOG_VIEW_H_
