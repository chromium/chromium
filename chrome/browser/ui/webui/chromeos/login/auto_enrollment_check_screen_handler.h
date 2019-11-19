// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTO_ENROLLMENT_CHECK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTO_ENROLLMENT_CHECK_SCREEN_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

// WebUI implementation of AutoEnrollmentCheckScreenActor.
class AutoEnrollmentCheckScreenHandler : public AutoEnrollmentCheckScreenView,
                                         public BaseScreenHandler {
 public:
  using TView = AutoEnrollmentCheckScreenView;

  explicit AutoEnrollmentCheckScreenHandler(
      JSCallsContainer* js_calls_container);
  ~AutoEnrollmentCheckScreenHandler() override;

  // AutoEnrollmentCheckScreenActor implementation:
  void Show() override;
  void SetDelegate(Delegate* delegate) override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

 private:
  Delegate* delegate_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;

  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentCheckScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_AUTO_ENROLLMENT_CHECK_SCREEN_HANDLER_H_

