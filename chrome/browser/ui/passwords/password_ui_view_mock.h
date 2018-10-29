// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_UI_VIEW_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_UI_VIEW_MOCK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPasswordUIView : public PasswordUIView {
 public:
  explicit MockPasswordUIView(Profile* profile);
  ~MockPasswordUIView() override;

  Profile* GetProfile() override;

  PasswordManagerPresenter* GetPasswordManagerPresenter();

#if !defined(OS_ANDROID)
  gfx::NativeWindow GetNativeWindow() const override;
#endif

  MOCK_METHOD2(ShowPassword, void(const std::string&, const base::string16&));

  MOCK_METHOD1(
      SetPasswordList,
      void(const std::vector<std::unique_ptr<autofill::PasswordForm>>&));

  MOCK_METHOD1(
      SetPasswordExceptionList,
      void(const std::vector<std::unique_ptr<autofill::PasswordForm>>&));

 private:
  Profile* profile_;
  PasswordManagerPresenter password_manager_presenter_;

  DISALLOW_COPY_AND_ASSIGN(MockPasswordUIView);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_UI_VIEW_MOCK_H_
