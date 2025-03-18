// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/credential_management/android/third_party_credential_manager_bridge.h"

#include <jni.h>

#include <memory>
#include <variant>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/credential_management/android/jni_headers/ThirdPartyCredentialManagerBridge_jni.h"

namespace credential_management {

using base::android::ConvertJavaStringToUTF8;
using JniDelegate = ThirdPartyCredentialManagerBridge::JniDelegate;

class JniDelegateImpl : public JniDelegate {
 public:
  JniDelegateImpl() = default;
  JniDelegateImpl(const JniDelegateImpl&) = delete;
  JniDelegateImpl& operator=(const JniDelegateImpl&) = delete;
  ~JniDelegateImpl() override = default;

  void CreateBridge(ThirdPartyCredentialManagerBridge* bridge) override {
    java_bridge_.Reset(Java_ThirdPartyCredentialManagerBridge_Constructor(
        jni_zero::AttachCurrentThread(), reinterpret_cast<intptr_t>(bridge)));
  }

  void Get(const std::string& origin) override {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    Java_ThirdPartyCredentialManagerBridge_get(
        env, java_bridge_, base::android::ConvertUTF8ToJavaString(env, origin));
  }

  void Store(const std::string& username,
             const std::string& password,
             const std::string& origin) override {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    Java_ThirdPartyCredentialManagerBridge_store(
        env, java_bridge_,
        base::android::ConvertUTF8ToJavaString(env, username),
        base::android::ConvertUTF8ToJavaString(env, password),
        base::android::ConvertUTF8ToJavaString(env, origin));
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

void ThirdPartyCredentialManagerBridge::Create(
    std::variant<GetCallback, StoreCallback> callback) {
  callback_ = std::move(callback);
  jni_delegate_->CreateBridge(this);
}

void ThirdPartyCredentialManagerBridge::Get(const std::string& origin) {
  jni_delegate_->Get(origin);
}

void ThirdPartyCredentialManagerBridge::OnPasswordCredentialReceived(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_username,
    const base::android::JavaParamRef<jstring>& j_password,
    const base::android::JavaParamRef<jstring>& j_origin) {
  CHECK(j_username);
  CHECK(j_password);
  password_manager::CredentialInfo info = password_manager::CredentialInfo(
      ::password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
      /*id=*/base::android::ConvertJavaStringToUTF16(env, j_username),
      base::android::ConvertJavaStringToUTF16(env, j_username),
      /*icon=*/GURL(), base::android::ConvertJavaStringToUTF16(env, j_password),
      /*federation=*/
      url::SchemeHostPort(
          GURL(base::android::ConvertJavaStringToUTF16(j_origin))));
  CHECK(std::holds_alternative<GetCallback>(callback_));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(std::get<0>(callback_)),
                     password_manager::CredentialManagerError::SUCCESS,
                     std::optional(info)));
}

void ThirdPartyCredentialManagerBridge::OnGetPasswordCredentialError(
    JNIEnv* env) {
  CHECK(std::holds_alternative<GetCallback>(callback_));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(std::get<0>(callback_)),
                     password_manager::CredentialManagerError::UNKNOWN,
                     std::nullopt));
}

void ThirdPartyCredentialManagerBridge::Store(const std::string& username,
                                              const std::string& password,
                                              const std::string& origin) {
  jni_delegate_->Store(username, password, origin);
}

void ThirdPartyCredentialManagerBridge::OnCreateCredentialResponse(
    JNIEnv* env,
    jboolean success) {
  CHECK(std::holds_alternative<StoreCallback>(callback_));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(std::get<1>(callback_))));
}

}  // namespace credential_management
