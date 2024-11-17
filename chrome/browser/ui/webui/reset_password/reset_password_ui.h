// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_RESET_PASSWORD_RESET_PASSWORD_UI_H_
#define CHROME_BROWSER_UI_WEBUI_RESET_PASSWORD_RESET_PASSWORD_UI_H_

#include "base/values.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ui/webui/reset_password/reset_password.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

using password_manager::metrics_util::PasswordType;

class ResetPasswordUI;

class ResetPasswordUIConfig
    : public content::DefaultWebUIConfig<ResetPasswordUI> {
 public:
  ResetPasswordUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIResetPasswordHost) {}
};

// The WebUI for chrome://reset-password/.
class ResetPasswordUI : public ui::MojoWebUIController {
 public:
  explicit ResetPasswordUI(content::WebUI* web_ui);

  ResetPasswordUI(const ResetPasswordUI&) = delete;
  ResetPasswordUI& operator=(const ResetPasswordUI&) = delete;

  ~ResetPasswordUI() override;

  // Instantiates the implementor of the mojom::ResetPasswordHandler mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<mojom::ResetPasswordHandler> receiver);

 private:
  base::Value::Dict PopulateStrings() const;

  std::unique_ptr<mojom::ResetPasswordHandler> ui_handler_;
  const PasswordType password_type_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_RESET_PASSWORD_RESET_PASSWORD_UI_H_
