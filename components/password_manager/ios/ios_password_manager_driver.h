// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_IOS_PASSWORD_MANAGER_DRIVER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_IOS_PASSWORD_MANAGER_DRIVER_H_

#include <vector>

#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/ios/password_manager_driver_bridge.h"

namespace autofill {
struct PasswordFormFillData;
}  // namespace autofill

namespace password_manager {
class PasswordAutofillManager;
class PasswordManager;
}  // namespace password_manager

// An iOS implementation of password_manager::PasswordManagerDriver.
class IOSPasswordManagerDriver
    : public password_manager::PasswordManagerDriver {
 public:
  explicit IOSPasswordManagerDriver(
      id<PasswordManagerDriverBridge> bridge,
      password_manager::PasswordManager* password_manager);

  IOSPasswordManagerDriver(const IOSPasswordManagerDriver&) = delete;
  IOSPasswordManagerDriver& operator=(const IOSPasswordManagerDriver&) = delete;

  ~IOSPasswordManagerDriver() override;

  // password_manager::PasswordManagerDriver implementation.
  int GetId() const override;
  void FillPasswordForm(
      const autofill::PasswordFormFillData& form_data) override;
  void InformNoSavedCredentials(
      bool should_show_popup_without_passwords) override;
  void FormEligibleForGenerationFound(
      const autofill::PasswordFormGenerationData& form) override;
  void GeneratedPasswordAccepted(const std::u16string& password) override;
  void FillSuggestion(const std::u16string& username,
                      const std::u16string& password) override;
  void PreviewSuggestion(const std::u16string& username,
                         const std::u16string& password) override;
  void ClearPreviewedForm() override;
  password_manager::PasswordGenerationFrameHelper* GetPasswordGenerationHelper()
      override;
  password_manager::PasswordManager* GetPasswordManager() override;
  password_manager::PasswordAutofillManager* GetPasswordAutofillManager()
      override;
  ::ui::AXTreeID GetAxTreeId() const override;
  bool IsInPrimaryMainFrame() const override;
  bool CanShowAutofillUi() const override;
  const GURL& GetLastCommittedURL() const override;

 private:
  __weak id<PasswordManagerDriverBridge> bridge_;  // (weak)
  password_manager::PasswordManager* password_manager_;
  std::unique_ptr<password_manager::PasswordGenerationFrameHelper>
      password_generation_helper_;
};

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_IOS_PASSWORD_MANAGER_DRIVER_H_
