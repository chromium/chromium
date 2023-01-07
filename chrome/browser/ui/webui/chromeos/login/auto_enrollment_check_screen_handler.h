// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTO_ENROLLMENT_CHECK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTO_ENROLLMENT_CHECK_SCREEN_HANDLER_H_

#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

// WebUI implementation of AutoEnrollmentCheckScreenView.
class AutoEnrollmentCheckScreenHandler : public AutoEnrollmentCheckScreenView,
                                         public BaseScreenHandler {
 public:
  using TView = AutoEnrollmentCheckScreenView;

  AutoEnrollmentCheckScreenHandler();

  AutoEnrollmentCheckScreenHandler(const AutoEnrollmentCheckScreenHandler&) =
      delete;
  AutoEnrollmentCheckScreenHandler& operator=(
      const AutoEnrollmentCheckScreenHandler&) = delete;

  ~AutoEnrollmentCheckScreenHandler() override = default;

  // AutoEnrollmentCheckScreenView:
  void Show() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::AutoEnrollmentCheckScreenHandler;
}

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTO_ENROLLMENT_CHECK_SCREEN_HANDLER_H_
