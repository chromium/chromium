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
#include "components/password_manager/core/common/credential_manager_types.h"

namespace credential_management {
using StoreCallback = base::OnceCallback<void()>;
using GetCallback = base::OnceCallback<void(
    password_manager::CredentialManagerError,
    const std::optional<password_manager::CredentialInfo>&)>;

// This class is a bridge between the browser and the Android Credential
// Manager. It allows the browser to get credentials from the Android Credential
// Manager.
class ThirdPartyCredentialManagerBridge {
 public:
  // This class allows to mock/fake the actual JNI calls. The implementation
  // should perform no work other than JNI calls. No logic, no conditions.
  class JniDelegate {
   public:
    virtual ~JniDelegate() = default;

    // Creates the JNI bridge.
    virtual void CreateBridge(ThirdPartyCredentialManagerBridge* bridge) = 0;

    // Gets a credential from the Android Credential Manager.
    virtual void Get(const std::string& origin) = 0;

    // Stores a credential to the Android Credential Manager.
    virtual void Store(const std::string& username,
                       const std::string& password,
                       const std::string& origin) = 0;
  };

  ThirdPartyCredentialManagerBridge();
  explicit ThirdPartyCredentialManagerBridge(
      base::PassKey<class ThirdPartyCredentialManagerBridgeTest>,
      std::unique_ptr<JniDelegate> jni_delegate);

  ThirdPartyCredentialManagerBridge(const ThirdPartyCredentialManagerBridge&) =
      delete;
  ThirdPartyCredentialManagerBridge& operator=(
      const ThirdPartyCredentialManagerBridge&) = delete;

  ~ThirdPartyCredentialManagerBridge();

  void Create(std::variant<GetCallback, StoreCallback> callback);

  void Get(const std::string& origin);
  void OnPasswordCredentialReceived(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_username,
      const base::android::JavaParamRef<jstring>& j_password,
      const base::android::JavaParamRef<jstring>& j_origin);
  void OnGetPasswordCredentialError(JNIEnv* env);

  void Store(const std::string& username,
             const std::string& password,
             const std::string& origin);
  void OnCreateCredentialResponse(JNIEnv* env, jboolean success);

 private:
  // TODO(crbug.com/404505860): Pass the callback to Java instead of having it
  // as a member.
  std::variant<GetCallback, StoreCallback> callback_;
  // Forwards all requests to JNI. Can be replaced in tests.
  std::unique_ptr<JniDelegate> jni_delegate_;
};

}  // namespace credential_management

#endif  // COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_THIRD_PARTY_CREDENTIAL_MANAGER_BRIDGE_H_
