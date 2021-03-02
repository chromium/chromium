// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SSH_CONFIGURED_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SSH_CONFIGURED_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"

namespace chromeos {

class JSCallsContainer;

// A class that handles getIsSshConfigured requests.
class SshConfiguredHandler : public BaseWebUIHandler {
 public:
  explicit SshConfiguredHandler(JSCallsContainer* js_calls_container);
  SshConfiguredHandler(const SshConfiguredHandler&) = delete;
  SshConfiguredHandler& operator=(const SshConfiguredHandler&) = delete;

  ~SshConfiguredHandler() override;

  // BaseWebUIHandler:
  void DeclareJSCallbacks() override;
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

 private:
  void HandleGetIsSshConfigured(const std::string& callback_id);
  void OnGetDebuggingFeatures(bool succeeded, int feature_mask);
  void ResolveCallbacks();

  base::Optional<bool> is_ssh_configured_;
  std::vector<std::string> callback_ids_;

  base::WeakPtrFactory<SshConfiguredHandler> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SSH_CONFIGURED_HANDLER_H_
