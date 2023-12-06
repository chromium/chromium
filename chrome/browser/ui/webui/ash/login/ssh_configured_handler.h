// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SSH_CONFIGURED_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SSH_CONFIGURED_HANDLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_webui_handler.h"

namespace ash {

// A class that handles getIsSshConfigured requests.
class SshConfiguredHandler : public BaseWebUIHandler {
 public:
  SshConfiguredHandler();
  SshConfiguredHandler(const SshConfiguredHandler&) = delete;
  SshConfiguredHandler& operator=(const SshConfiguredHandler&) = delete;

  ~SshConfiguredHandler() override;

  // BaseWebUIHandler:
  void DeclareJSCallbacks() override;
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void InitAfterJavascriptAllowed() final;

 private:
  void HandleGetIsSshConfigured(const std::string& callback_id);
  void OnGetDebuggingFeatures(bool succeeded, int feature_mask);
  void ResolveCallbacks();

  std::optional<bool> is_ssh_configured_;
  std::vector<std::string> callback_ids_;

  base::WeakPtrFactory<SshConfiguredHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SSH_CONFIGURED_HANDLER_H_
