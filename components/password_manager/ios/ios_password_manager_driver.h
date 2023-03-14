// Copyright 2022 The Chromium Authors
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

namespace web {
class WebFrame;
}  // namespace web

// An iOS implementation of password_manager::PasswordManagerDriver.
class IOSPasswordManagerDriver
    : public password_manager::PasswordManagerDriver,
      public base::RefCountedThreadSafe<IOSPasswordManagerDriver> {
 public:
  IOSPasswordManagerDriver(const IOSPasswordManagerDriver&) = delete;
  IOSPasswordManagerDriver& operator=(const IOSPasswordManagerDriver&) = delete;

  // password_manager::PasswordManagerDriver implementation.
  int GetId() const override;
  void SetPasswordFillData(
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
  void PreviewGenerationSuggestion(const std::u16string& password) override;
  void ClearPreviewedForm() override;
  void SetSuggestionAvailability(
      autofill::FieldRendererId generation_element_id,
      const autofill::mojom::AutofillState state) override;
  password_manager::PasswordGenerationFrameHelper* GetPasswordGenerationHelper()
      override;
  password_manager::PasswordManagerInterface* GetPasswordManager() override;
  password_manager::PasswordAutofillManager* GetPasswordAutofillManager()
      override;
  ::ui::AXTreeID GetAxTreeId() const override;
  int GetFrameId() const override;
  bool IsInPrimaryMainFrame() const override;
  bool CanShowAutofillUi() const override;
  const GURL& GetLastCommittedURL() const override;
  // In some cases the web frame might not exist anymore (when the frame is
  // deleted by the webpage straight after form submission, but the driver is
  // still alive). So only use this getter when you are sure that the frame
  // still exists.
  web::WebFrame* web_frame() const { return web_frame_; }
  const GURL& security_origin() const { return security_origin_; }
  void ProcessFrameDeletion();

 private:
  // The constructor below is private so that no one uses it while trying to
  // create/get a driver. However, IOSPasswordManagerWebFrameDriverHelper needs
  // to be able to access it in the driver creation flow.
  friend class IOSPasswordManagerWebFrameDriverHelper;
  friend class base::RefCountedThreadSafe<IOSPasswordManagerDriver>;

  // To create a new driver, use
  // IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame.
  IOSPasswordManagerDriver(
      id<PasswordManagerDriverBridge> bridge,
      password_manager::PasswordManagerInterface* password_manager,
      web::WebFrame* web_frame,
      int driver_id);

  ~IOSPasswordManagerDriver() override;

  __weak id<PasswordManagerDriverBridge> bridge_;  // (weak)
  password_manager::PasswordManagerInterface* password_manager_;
  std::unique_ptr<password_manager::PasswordGenerationFrameHelper>
      password_generation_helper_;
  web::WebFrame* web_frame_;
  int id_;

  // The hash of the cached frame ID of `web_frame_`. This is cached because
  // `web_frame` might be set to null when the frame is deleted.
  int cached_frame_id_;

  bool is_in_main_frame_;
  // The security origin associated with |web_frame_|.
  GURL security_origin_;
};

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_IOS_PASSWORD_MANAGER_DRIVER_H_
