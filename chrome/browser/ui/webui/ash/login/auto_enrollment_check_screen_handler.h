// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_AUTO_ENROLLMENT_CHECK_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_AUTO_ENROLLMENT_CHECK_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen_view.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

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

  ~AutoEnrollmentCheckScreenHandler() override;

  // AutoEnrollmentCheckScreenView:
  void Show() override;
  base::WeakPtr<AutoEnrollmentCheckScreenView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

 private:
  base::WeakPtrFactory<AutoEnrollmentCheckScreenHandler> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_AUTO_ENROLLMENT_CHECK_SCREEN_HANDLER_H_
