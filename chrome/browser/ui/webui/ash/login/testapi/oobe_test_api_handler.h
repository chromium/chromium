// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TESTAPI_OOBE_TEST_API_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TESTAPI_OOBE_TEST_API_HANDLER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_webui_handler.h"

class PrefService;

namespace metrics {
class MetricsService;
}  // namespace metrics

namespace ash {

class OobeTestAPIHandler : public BaseWebUIHandler {
 public:
  // TODO(crbug.com/558489113): Revisit callback usage once lifetime is resolved
  // to inject raw_ref/raw_ptr directly.
  using MetricsServiceGetter =
      base::RepeatingCallback<::metrics::MetricsService*()>;

  // `local_state` must be non-null and must outlive `this`.
  // `metrics_service_getter` must be non-null.
  OobeTestAPIHandler(PrefService* local_state,
                     MetricsServiceGetter metrics_service_getter);
  ~OobeTestAPIHandler() override;
  OobeTestAPIHandler(const OobeTestAPIHandler&) = delete;
  OobeTestAPIHandler& operator=(const OobeTestAPIHandler&) = delete;

  // WebUIMessageHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;
  void GetAdditionalParameters(base::DictValue* dict) override;

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
  void HandleGetShouldSkipSplitModifierScreen(const std::string& callback_id);

  const raw_ref<PrefService> local_state_;
  const MetricsServiceGetter metrics_service_getter_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_TESTAPI_OOBE_TEST_API_HANDLER_H_
