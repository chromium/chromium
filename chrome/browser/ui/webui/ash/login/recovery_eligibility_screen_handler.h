// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_RECOVERY_ELIGIBILITY_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_RECOVERY_ELIGIBILITY_SCREEN_HANDLER_H_

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

class RecoveryEligibilityView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "recovery-check", "RecoveryEligibilityScreen"};

  RecoveryEligibilityView() = default;
  virtual ~RecoveryEligibilityView() = default;

  RecoveryEligibilityView(const RecoveryEligibilityView&) = delete;
  RecoveryEligibilityView& operator=(const RecoveryEligibilityView&) = delete;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_RECOVERY_ELIGIBILITY_SCREEN_HANDLER_H_
