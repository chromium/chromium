// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_IOS_PASSWORD_MANAGER_DRIVER_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_IOS_PASSWORD_MANAGER_DRIVER_H_

#include <vector>

#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/ios/password_manager_driver_bridge.h"
#include "url/gurl.h"

namespace autofill {
struct PasswordFormFillData;
}  // namespace autofill

namespace password_manager {
class PasswordAutofillManager;
class PasswordManager;
}  // namespace password_manager

namespace web {
class WebFrame;
class WebState;
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
  const std::string& web_frame_id() const { return frame_id_; }
  const GURL& security_origin() const { return security_origin_; }

 private:
  // The constructor below is private so that no one uses it while trying to
  // create/get a driver. However, IOSPasswordManagerWebFrameDriverHelper needs
  // to be able to access it in the driver creation flow.
  friend class IOSPasswordManagerWebFrameDriverHelper;
  friend class base::RefCountedThreadSafe<IOSPasswordManagerDriver>;

  // To create a new driver, use
  // IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame.
  IOSPasswordManagerDriver(
      web::WebState* web_state,
      id<PasswordManagerDriverBridge> bridge,
      password_manager::PasswordManagerInterface* password_manager,
      web::WebFrame* web_frame,
      int driver_id);

  ~IOSPasswordManagerDriver() override;

  base::WeakPtr<web::WebState> web_state_;
  __weak id<PasswordManagerDriverBridge> bridge_;  // (weak)
  password_manager::PasswordManagerInterface* password_manager_;
  std::unique_ptr<password_manager::PasswordGenerationFrameHelper>
      password_generation_helper_;
  int id_;

  // The hash of the cached frame ID of `web_frame_`. This is cached because
  // `web_frame` might be set to null when the frame is deleted.
  int cached_frame_id_;

  // The frame ID of `web_frame_`. This is used to get the web frame associated
  // to it and determine if it is still a valid web frame. See `web_frame_`
  // comment: the driver can outlive the `web_frame()`. This can happen when the
  // driver is handling the saving, editing or syncing of the password after a
  // form submission.
  std::string frame_id_;

  bool is_in_main_frame_;
  // The security origin associated with |web_frame_|.
  GURL security_origin_;
};

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_IOS_PASSWORD_MANAGER_DRIVER_H_
