// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_UI_VIEW_H_
#define CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_UI_VIEW_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

// An interface for a passwords UI View. A UI view is responsible for
// displaying passwords in the UI and routing UI commands to the
// PasswordManagerPresenter.
class PasswordUIView {
 public:
  virtual ~PasswordUIView() {}

  // Returns the profile associated with the currently active profile.
  virtual Profile* GetProfile() = 0;

  // Updates the list of passwords in the UI.
  // |password_list| the list of saved password entries.
  // |show_passwords| true if the passwords should be shown in the UI.
  virtual void SetPasswordList(
      const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
          password_list) = 0;

  // Updates the list of password exceptions in the UI.
  // |password_exception_list| The list of saved password exceptions.
  virtual void SetPasswordExceptionList(
      const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
          password_exception_list) = 0;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_SETTINGS_PASSWORD_UI_VIEW_H_
