// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_THIRD_PARTY_CREDENTIAL_MANAGER_BRIDGE_H_
#define COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_THIRD_PARTY_CREDENTIAL_MANAGER_BRIDGE_H_

#include <jni.h>

#include <variant>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/types/pass_key.h"
#include "components/credential_management/android/password_credential_response.h"
#include "components/password_manager/core/common/credential_manager_types.h"

namespace credential_management {
using StoreCallback = base::OnceCallback<void()>;
using GetCallback = base::OnceCallback<void(
    password_manager::CredentialManagerError,
    const std::optional<password_manager::CredentialInfo>&)>;

class CredentialManagerBridge {
 public:
  virtual ~CredentialManagerBridge() = default;

  virtual void Get(bool is_auto_select_allowed,
                   bool include_passwords,
                   const std::string& origin,
                   GetCallback completion_callback) = 0;

  virtual void Store(const std::u16string& username,
                     const std::u16string& password,
                     const std::string& origin,
                     StoreCallback completion_callback) = 0;
};

// This class is a bridge between the browser and the Android Credential
// Manager. It allows the browser to get credentials from the Android Credential
// Manager.
class ThirdPartyCredentialManagerBridge : public CredentialManagerBridge {
 public:
  // This class allows to mock/fake the actual JNI calls. The implementation
  // should perform no work other than JNI calls. No logic, no conditions.
  class JniDelegate {
   public:
    virtual ~JniDelegate() = default;

    // Creates the JNI bridge.
    virtual void CreateBridge() = 0;

    // Gets a credential from the Android Credential Manager.
    // The `completion_callback` should always be invoked on completion, passing
    // the PasswordCredentialResponse.
    virtual void Get(bool is_auto_select_allowed,
                     bool include_passwords,
                     const std::string& origin,
                     base::OnceCallback<void(PasswordCredentialResponse)>
                         completion_callback) = 0;

    // Stores a credential to the Android Credential Manager.
    // The `completion_callback` should always be invoked on completion, passing
    // a success status.
    virtual void Store(const std::u16string& username,
                       const std::u16string& password,
                       const std::string& origin,
                       base::OnceCallback<void(bool)> completion_callback) = 0;
  };

  ThirdPartyCredentialManagerBridge();
  explicit ThirdPartyCredentialManagerBridge(
      base::PassKey<class ThirdPartyCredentialManagerBridgeTest>,
      std::unique_ptr<JniDelegate> jni_delegate);

  ThirdPartyCredentialManagerBridge(const ThirdPartyCredentialManagerBridge&) =
      delete;
  ThirdPartyCredentialManagerBridge& operator=(
      const ThirdPartyCredentialManagerBridge&) = delete;

  ~ThirdPartyCredentialManagerBridge() override;

  void Create();

  void Get(bool is_auto_select_allowed,
           bool include_passwords,
           const std::string& origin,
           GetCallback completion_callback) override;

  void Store(const std::u16string& username,
             const std::u16string& password,
             const std::string& origin,
             StoreCallback completion_callback) override;

 private:
  // Forwards all requests to JNI. Can be replaced in tests.
  std::unique_ptr<JniDelegate> jni_delegate_;
};

}  // namespace credential_management

#endif  // COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_THIRD_PARTY_CREDENTIAL_MANAGER_BRIDGE_H_
