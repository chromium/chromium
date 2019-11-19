// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision.mojom.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_handler.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/label.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "url/gurl.h"

namespace chromeos {

// Dialog which displays the add-supervision flow which allows users to
// convert a regular Google account into a Family-Link managed account.
class AddSupervisionDialog : public SystemWebDialogDelegate {
 public:
  // Shows the dialog; if the dialog is already displayed, this function is a
  // no-op.
  static void Show(gfx::NativeView parent);

  static SystemWebDialogDelegate* GetInstance();

  // Closes the dialog; if the dialog doesn't exist, this function is a
  // no-op.
  // This is only called when the user clicks "Cancel", not the "x" in the top
  // right.
  static void Close();

  // Deletes this dialog window.
  // Currently only used by AddSupervisionMetricsRecorderTest browser test to
  // simulate closing the dialog cleanly.
  void CloseNowForTesting();

  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  void GetDialogSize(gfx::Size* size) const override;
  bool CanCloseDialog() const override;
  bool OnDialogCloseRequested() override;

 protected:
  AddSupervisionDialog();
  ~AddSupervisionDialog() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AddSupervisionDialog);
};

// Controller for chrome://add-supervision
class AddSupervisionUI : public ui::MojoWebUIController,
                         public AddSupervisionHandler::Delegate {
 public:
  explicit AddSupervisionUI(content::WebUI* web_ui);
  ~AddSupervisionUI() override;

  // AddSupervisionHandler::Delegate:
  bool CloseDialog() override;

  static void SetUpForTest(signin::IdentityManager* identity_manager);

 private:
  void BindAddSupervisionHandler(
      mojo::PendingReceiver<add_supervision::mojom::AddSupervisionHandler>
          receiver);
  void SetUpResources();
  GURL GetAddSupervisionURL();

  std::unique_ptr<add_supervision::mojom::AddSupervisionHandler>
      mojo_api_handler_;

  GURL supervision_url_;

  static signin::IdentityManager* test_identity_manager_;
  bool allow_non_google_url_for_tests_ = false;

  DISALLOW_COPY_AND_ASSIGN(AddSupervisionUI);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_UI_H_
