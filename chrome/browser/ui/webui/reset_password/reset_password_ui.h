// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_RESET_PASSWORD_RESET_PASSWORD_UI_H_
#define CHROME_BROWSER_UI_WEBUI_RESET_PASSWORD_RESET_PASSWORD_UI_H_

#include "base/macros.h"
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ui/webui/reset_password/reset_password.mojom.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace base {
class DictionaryValue;
}

using password_manager::metrics_util::PasswordType;

// The WebUI for chrome://reset-password/.
class ResetPasswordUI : public ui::MojoWebUIController {
 public:
  explicit ResetPasswordUI(content::WebUI* web_ui);
  ~ResetPasswordUI() override;

 private:
  void BindResetPasswordHandler(
      mojo::PendingReceiver<mojom::ResetPasswordHandler> receiver);

  base::DictionaryValue PopulateStrings() const;

  std::unique_ptr<mojom::ResetPasswordHandler> ui_handler_;
  const PasswordType password_type_;

  DISALLOW_COPY_AND_ASSIGN(ResetPasswordUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_RESET_PASSWORD_RESET_PASSWORD_UI_H_
