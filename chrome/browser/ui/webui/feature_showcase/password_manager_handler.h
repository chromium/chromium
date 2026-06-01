// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_PASSWORD_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_PASSWORD_MANAGER_HANDLER_H_

#include "chrome/browser/ui/webui/feature_showcase/password_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

class PasswordManagerHandler
    : public feature_showcase::mojom::PasswordManagerPageHandler {
 public:
  PasswordManagerHandler(
      mojo::PendingReceiver<feature_showcase::mojom::PasswordManagerPageHandler>
          receiver,
      Profile* profile);
  PasswordManagerHandler(const PasswordManagerHandler&) = delete;
  PasswordManagerHandler& operator=(const PasswordManagerHandler&) = delete;
  ~PasswordManagerHandler() override;

  // feature_showcase::mojom::PasswordManagerPageHandler:
  void PinPasswordManager() override;

 private:
  mojo::Receiver<feature_showcase::mojom::PasswordManagerPageHandler> receiver_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_PASSWORD_MANAGER_HANDLER_H_
