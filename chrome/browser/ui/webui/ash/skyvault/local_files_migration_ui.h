// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_UI_H_

#include "base/memory/weak_ptr.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/webui_config.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace policy::local_user_files {

class LocalFilesMigrationUI;

// WebUIConfig for chrome://local-files-migration
class LocalFilesMigrationUIConfig
    : public content::DefaultWebUIConfig<LocalFilesMigrationUI> {
 public:
  LocalFilesMigrationUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUILocalFilesMigrationHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// Provides the WebUI for the LocalFilesMigrationDialog.
class LocalFilesMigrationUI : public ui::WebDialogUI {
 public:
  explicit LocalFilesMigrationUI(content::WebUI* web_ui);
  LocalFilesMigrationUI(const LocalFilesMigrationUI&) = delete;
  LocalFilesMigrationUI& operator=(const LocalFilesMigrationUI&) = delete;
  ~LocalFilesMigrationUI() override;

 private:
  base::WeakPtrFactory<LocalFilesMigrationUI> weak_factory_{this};

  // Called if the user clicks on "Upload now".
  void HandleStartMigration(const base::Value::List& args);

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_UI_H_
