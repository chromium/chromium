// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TESTAPI_OOBE_TEST_API_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TESTAPI_OOBE_TEST_API_HANDLER_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"

namespace chromeos {

class OobeTestAPIHandler : public BaseWebUIHandler {
 public:
  explicit OobeTestAPIHandler(JSCallsContainer* js_calls_container);
  ~OobeTestAPIHandler() override;
  OobeTestAPIHandler(const OobeTestAPIHandler&) = delete;
  OobeTestAPIHandler& operator=(const OobeTestAPIHandler&) = delete;

  // WebUIMessageHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_TESTAPI_OOBE_TEST_API_HANDLER_H_
