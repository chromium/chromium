// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_UI_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration.mojom-shared.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration.mojom.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}

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
class LocalFilesMigrationUI : public ui::MojoWebDialogUI,
                              public mojom::PageHandlerFactory {
 public:
  explicit LocalFilesMigrationUI(content::WebUI* web_ui);
  LocalFilesMigrationUI(const LocalFilesMigrationUI&) = delete;
  LocalFilesMigrationUI& operator=(const LocalFilesMigrationUI&) = delete;
  ~LocalFilesMigrationUI() override;

  // Binds the Mojo interface for PageHandlerFactory.
  void BindInterface(mojo::PendingReceiver<mojom::PageHandlerFactory> receiver);

  // Binds the color change handler.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  // mojom::PageHandlerFactory implementation:
  // Creates a PageHandler to handle communication with the WebUI page.
  void CreatePageHandler(
      mojo::PendingRemote<mojom::Page> page,
      mojo::PendingReceiver<mojom::PageHandler> pending_page_handler) override;

  // Sets the initial dialog parameters.
  void SetInitialDialogInfo(CloudProvider cloud_provider,
                            base::Time migration_start_time);

 private:
  // Processes the user's action and closes the dialog accordingly.
  void ProcessResponseAndCloseDialog(UserAction action);

  CloudProvider cloud_provider_;
  base::Time migration_start_time_;

  // Page handler for WebUI interaction
  std::unique_ptr<LocalFilesMigrationPageHandler> handler_;
  // Mojo communication
  mojo::Receiver<mojom::PageHandlerFactory> factory_receiver_{this};

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  base::WeakPtrFactory<LocalFilesMigrationUI> weak_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SKYVAULT_LOCAL_FILES_MIGRATION_UI_H_
