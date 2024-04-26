// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TESTAPI_OOBE_TEST_API_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TESTAPI_OOBE_TEST_API_HANDLER_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_webui_handler.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"

namespace ash {

class OobeTestAPIHandler : public BaseWebUIHandler {
 public:
  OobeTestAPIHandler();
  ~OobeTestAPIHandler() override;
  OobeTestAPIHandler(const OobeTestAPIHandler&) = delete;
  OobeTestAPIHandler& operator=(const OobeTestAPIHandler&) = delete;

  // WebUIMessageHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;

 private:
  void LoginWithPin(const std::string& username, const std::string& pin);
  void AdvanceToScreen(const std::string& screen);
  void SkipToLoginForTesting();
  void SkipPostLoginScreens();
  void HandleCompleteLogin(const std::string& gaia_id,
                           const std::string& typed_email,
                           const std::string& password);
  void LoginAsGuest();
  void ShowGaiaDialog();
  void HandleGetPrimaryDisplayName(const std::string& callback_id);
  void HandleGetShouldSkipChoobe(const std::string& callback_id);
  void HandleGetShouldSkipTouchpadScroll(const std::string& callback_id);
  void HandleGetMetricsClientID(const std::string& callback_id);

  void OnGetDisplayUnitInfoList(
      const std::string& callback_id,
      std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list);
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TESTAPI_OOBE_TEST_API_HANDLER_H_
