// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEBSITE_LOGIN_MANAGER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEBSITE_LOGIN_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill_assistant/browser/save_password_leak_detection_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace autofill_assistant {

// Common interface for implementations that fetch login details for websites.
class WebsiteLoginManager {
 public:
  // Uniquely represents a particular login.
  struct Login {
    Login(const GURL& origin, const std::string& username);
    Login(const Login& other);
    ~Login();

    // The origin of the login website.
    GURL origin;
    std::string username;
  };

  WebsiteLoginManager() = default;

  WebsiteLoginManager(const WebsiteLoginManager&) = delete;
  WebsiteLoginManager& operator=(const WebsiteLoginManager&) = delete;

  virtual ~WebsiteLoginManager() = default;

  // Asynchronously returns all matching login details for |url| in the
  // specified callback.
  virtual void GetLoginsForUrl(
      const GURL& url,
      base::OnceCallback<void(std::vector<Login>)> callback) = 0;

  // Retrieves the password for |login| in the specified |callback|, or |false|
  // if the password could not be retrieved.
  virtual void GetPasswordForLogin(
      const Login& login,
      base::OnceCallback<void(bool, std::string)> callback) = 0;

  // Deletes the password for |login|.
  virtual void DeletePasswordForLogin(
      const Login& login,
      base::OnceCallback<void(bool)> callback) = 0;

  // Edits the password for |login|.
  virtual void EditPasswordForLogin(
      const Login& login,
      const std::string& new_password,
      base::OnceCallback<void(bool)> callback) = 0;

  // Read the last date a password was used for |login|. In case no match is
  // found for the given login returns nullptr.
  virtual void GetGetLastTimePasswordUsed(
      const Login& login,
      base::OnceCallback<void(absl::optional<base::Time>)> callback) = 0;

  // Generates new strong password. |form/field_signature| are used to fetch
  // password requirements. |max_length| is the "max_length" attribute of input
  // field that limits the length of value. Returns |absl::nullopt| if the
  // password cannot be generated for some reason.
  virtual absl::optional<std::string> GeneratePassword(
      autofill::FormSignature form_signature,
      autofill::FieldSignature field_signature,
      uint64_t max_length) = 0;

  // Presaves generated passwod for the form. Password will be saved after
  // successful form submission.
  virtual void PresaveGeneratedPassword(
      const Login& login,
      const std::string& password,
      const autofill::FormData& form_data,
      base::OnceCallback<void()> callback) = 0;

  // Checks if generated password can be saved.
  virtual bool ReadyToSaveGeneratedPassword() = 0;

  // Saves the presaved passwod to the store.
  virtual void SaveGeneratedPassword() = 0;

  // Clears potentially submitted or pending forms in password manager. Used to
  // make password manager "forget" about any previously processed form that
  // is pending or submitted.
  virtual void ResetPendingCredentials() = 0;

  // Returns true if password manager has processed a password update submission
  // on a 3rd party website and it is ready to save the updated credential to
  // the password store.
  virtual bool ReadyToSaveSubmittedPassword() = 0;

  // Checks whether there is a password submission on the website and whether
  // the submission corresponds to a password update. In particular, it returns
  // false if the submitted password update is the same as the previously used
  // password.
  virtual bool SubmittedPasswordIsSame() = 0;

  // Checks whether the submitted credential is leaked. The result is returned
  // by calling a SavePasswordLeakDetectionDelegate::Callback with the first
  // parameter indicating whether the credential check was performed
  // successfully and the second parameter indicating whether the credential is
  // known to be leaked.
  virtual void CheckWhetherSubmittedCredentialIsLeaked(
      SavePasswordLeakDetectionDelegate::Callback callback,
      base::TimeDelta timeout) = 0;

  // Saves the current submitted password to the disk. Returns true if a
  // submitted password exist (e.g. ReadyToSaveSubmittedPassword) and it is
  // properly saved, false otherwise.
  virtual bool SaveSubmittedPassword() = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WEBSITE_LOGIN_MANAGER_H_
