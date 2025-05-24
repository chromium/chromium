// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/credential_management/android/third_party_credential_manager_bridge.h"

#include <jni.h>

#include <memory>
#include <variant>

#include "base/android/jni_callback.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/credential_management/android/jni_headers/ThirdPartyCredentialManagerBridge_jni.h"

namespace credential_management {

using base::android::ConvertJavaStringToUTF8;
using JniDelegate = ThirdPartyCredentialManagerBridge::JniDelegate;

void OnPasswordCredentialReceived(
    const std::string& origin,
    GetCallback completion_callback,
    PasswordCredentialResponse credential_response) {
  password_manager::CredentialManagerError status =
      password_manager::CredentialManagerError::UNKNOWN;
  std::optional<password_manager::CredentialInfo> info;
  if (credential_response.success) {
    status = password_manager::CredentialManagerError::SUCCESS;
    info = password_manager::CredentialInfo(
        ::password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
        /*id=*/credential_response.username,
        /*name=*/credential_response.username,
        /*icon=*/GURL(),
        /*password=*/credential_response.password,
        /*federation=*/
        url::SchemeHostPort(GURL(origin)));
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(completion_callback), status, std::move(info)));
}

void OnCreateCredentialResponse(StoreCallback completion_callback,
                                bool success) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion_callback)));
}

class JniDelegateImpl : public JniDelegate {
 public:
  JniDelegateImpl() = default;
  JniDelegateImpl(const JniDelegateImpl&) = delete;
  JniDelegateImpl& operator=(const JniDelegateImpl&) = delete;
  ~JniDelegateImpl() override = default;

  void CreateBridge() override {
    java_bridge_.Reset(Java_ThirdPartyCredentialManagerBridge_Constructor(
        jni_zero::AttachCurrentThread()));
  }

  void Get(bool is_auto_select_allowed,
           bool include_passwords,
           const std::string& origin,
           base::OnceCallback<void(PasswordCredentialResponse)>
               completion_callback) override {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    Java_ThirdPartyCredentialManagerBridge_get(
        env, java_bridge_, is_auto_select_allowed, include_passwords,
        base::android::ConvertUTF8ToJavaString(env, origin),
        base::android::ToJniCallback(env, std::move(completion_callback)));
  }

  void Store(const std::u16string& username,
             const std::u16string& password,
             const std::string& origin,
             base::OnceCallback<void(bool)> completion_callback) override {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    Java_ThirdPartyCredentialManagerBridge_store(
        env, java_bridge_,
        base::android::ConvertUTF16ToJavaString(env, username),
        base::android::ConvertUTF16ToJavaString(env, password),
        base::android::ConvertUTF8ToJavaString(env, origin),
        base::android::ToJniCallback(env, std::move(completion_callback)));
  }

 private:
  // The corresponding Java ThirdPartyCredentialManagerBridge.
  base::android::ScopedJavaLocalRef<jobject> java_bridge_;
};

ThirdPartyCredentialManagerBridge::ThirdPartyCredentialManagerBridge()
    : jni_delegate_(std::make_unique<JniDelegateImpl>()) {}

ThirdPartyCredentialManagerBridge::ThirdPartyCredentialManagerBridge(
    base::PassKey<class ThirdPartyCredentialManagerBridgeTest>,
    std::unique_ptr<JniDelegate> jni_delegate)
    : jni_delegate_(std::move(jni_delegate)) {}

ThirdPartyCredentialManagerBridge::~ThirdPartyCredentialManagerBridge() =
    default;

void ThirdPartyCredentialManagerBridge::Create() {
  jni_delegate_->CreateBridge();
}

void ThirdPartyCredentialManagerBridge::Get(bool is_auto_select_allowed,
                                            bool include_passwords,
                                            const std::string& origin,
                                            GetCallback completion_callback) {
  base::OnceCallback<void(PasswordCredentialResponse)> on_complete =
      base::BindOnce(&OnPasswordCredentialReceived, origin,
                     std::move(completion_callback));
  jni_delegate_->Get(is_auto_select_allowed, include_passwords, origin, std::move(on_complete));
}

void ThirdPartyCredentialManagerBridge::Store(
    const std::u16string& username,
    const std::u16string& password,
    const std::string& origin,
    StoreCallback completion_callback) {
  base::OnceCallback<void(bool)> on_complete = base::BindOnce(
      &OnCreateCredentialResponse, std::move(completion_callback));
  jni_delegate_->Store(username, password, origin, std::move(on_complete));
}

}  // namespace credential_management
