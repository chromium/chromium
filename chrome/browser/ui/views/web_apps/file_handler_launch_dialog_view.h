// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FILE_HANDLER_LAUNCH_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FILE_HANDLER_LAUNCH_DIALOG_VIEW_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/web_apps/launch_app_user_choice_dialog_view.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

// User choice dialog for PWA file handling, shown before launching a PWA to
// handle files. See https://web.dev/file-handling/
class FileHandlerLaunchDialogView : public LaunchAppUserChoiceDialogView {
 public:
  METADATA_HEADER(FileHandlerLaunchDialogView);

  FileHandlerLaunchDialogView(
      const std::vector<base::FilePath>& file_paths,
      Profile* profile,
      const AppId& app_id,
      chrome::WebAppLaunchAcceptanceCallback close_callback);

  FileHandlerLaunchDialogView(const FileHandlerLaunchDialogView&) = delete;
  FileHandlerLaunchDialogView& operator=(const FileHandlerLaunchDialogView&) =
      delete;
  ~FileHandlerLaunchDialogView() override;

 protected:
  std::unique_ptr<views::View> CreateAboveAppInfoView() override;
  std::unique_ptr<views::View> CreateBelowAppInfoView() override;
  std::u16string GetRememberChoiceString() override;

 private:
  const std::vector<base::FilePath> file_paths_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FILE_HANDLER_LAUNCH_DIALOG_VIEW_H_
