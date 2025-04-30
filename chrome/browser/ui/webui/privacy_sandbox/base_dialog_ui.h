// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_BASE_DIALOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_BASE_DIALOG_UI_H_

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog.mojom.h"
#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace privacy_sandbox {

// Defines the interface for native dialog operations that the WebUI layer can
// trigger. Implemented by PrivacySandboxDialogView, this delegate allows the
// BaseDialogHandler to control the native dialog without a direct dependency on
// the view implementation.
class BaseDialogUIDelegate {
 public:
  virtual ~BaseDialogUIDelegate() = default;

  virtual void ResizeNativeView(int height) = 0;
  virtual void ShowNativeView() = 0;
  virtual void CloseNativeView() = 0;
};

// MojoWebUIController for Privacy Sandbox Base Dialog
class BaseDialogUI : public ui::MojoWebUIController {
 public:
  explicit BaseDialogUI(content::WebUI* web_ui);
  BaseDialogUI(const BaseDialogUI&) = delete;
  BaseDialogUI& operator=(const BaseDialogUI&) = delete;

  ~BaseDialogUI() override;

  // Instantiates the implementor of the
  // privacy_sandbox::dialog::mojom::BaseDialogPageHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<dialog::mojom::BaseDialogPageHandler> receiver);

 private:
  std::unique_ptr<BaseDialogHandler> page_handler_;
  raw_ptr<BaseDialogUIDelegate> delegate_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class BaseDialogUIConfig : public content::DefaultWebUIConfig<BaseDialogUI> {
 public:
  BaseDialogUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIPrivacySandboxBaseDialogHost) {}
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_BASE_DIALOG_UI_H_
