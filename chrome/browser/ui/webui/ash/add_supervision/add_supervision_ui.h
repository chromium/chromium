// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_

#include <memory>

#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_handler.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/label.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "url/gurl.h"

namespace ash {

// Dialog which displays the add-supervision flow which allows users to
// convert a regular Google account into a Family-Link managed account.
class AddSupervisionDialog : public SystemWebDialogDelegate {
 public:
  AddSupervisionDialog(const AddSupervisionDialog&) = delete;
  AddSupervisionDialog& operator=(const AddSupervisionDialog&) = delete;

  // Shows the dialog; if the dialog is already displayed, this function is a
  // no-op.
  static void Show();

  static AddSupervisionDialog* GetInstance();

  // Closes the dialog; if the dialog doesn't exist, this function is a
  // no-op.
  // This is only called when the user clicks "Cancel", not the "x" in the top
  // right.
  static void Close();

  // Updates the ShouldCloseDialogOnEscape() state (i.e., whether pressing
  // Escape closes the main dialog).
  static void SetCloseOnEscape(bool enabled);

  // Deletes this dialog window.
  // Currently only used by AddSupervisionMetricsRecorderTest browser test to
  // simulate closing the dialog cleanly.
  void CloseNowForTesting();

  // ui::WebDialogDelegate:
  ui::mojom::ModalType GetDialogModalType() const override;
  void GetDialogSize(gfx::Size* size) const override;
  bool OnDialogCloseRequested() override;
  void OnDialogWillClose() override;
  bool ShouldCloseDialogOnEscape() const override;
  bool ShouldShowDialogTitle() const override;

 protected:
  AddSupervisionDialog();
  ~AddSupervisionDialog() override;

 private:
  bool should_close_on_escape_ = true;
};

class AddSupervisionUI;

// WebUIConfig for chrome://add-supervision
class AddSupervisionUIConfig
    : public content::DefaultWebUIConfig<AddSupervisionUI> {
 public:
  AddSupervisionUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIAddSupervisionHost) {}
};

// Controller for chrome://add-supervision
class AddSupervisionUI : public ui::MojoWebUIController,
                         public AddSupervisionHandler::Delegate {
 public:
  explicit AddSupervisionUI(content::WebUI* web_ui);

  AddSupervisionUI(const AddSupervisionUI&) = delete;
  AddSupervisionUI& operator=(const AddSupervisionUI&) = delete;

  ~AddSupervisionUI() override;

  // AddSupervisionHandler::Delegate:
  bool CloseDialog() override;
  void SetCloseOnEscape(bool) override;

  static void SetUpForTest(signin::IdentityManager* identity_manager);

  // Instantiates the implementor of the mojom::AddSupervisionHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<add_supervision::mojom::AddSupervisionHandler>
          receiver);

 private:
  void SetUpResources();
  GURL GetAddSupervisionURL();

  std::unique_ptr<add_supervision::mojom::AddSupervisionHandler>
      mojo_api_handler_;

  GURL supervision_url_;

  static signin::IdentityManager* test_identity_manager_;
  bool allow_non_google_url_for_tests_ = false;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_
