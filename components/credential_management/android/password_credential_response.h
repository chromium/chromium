// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_PASSWORD_CREDENTIAL_RESPONSE_H_
#define COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_PASSWORD_CREDENTIAL_RESPONSE_H_

#include <string>

#include "base/component_export.h"
#include "third_party/jni_zero/jni_zero.h"

namespace credential_management {

// This class is the C++ version of the Java class
// org.chromium.components.credential_management.PasswordCredentialResponse. It
// is used to pass password credentials that were requested through Credential
// Management API.
struct COMPONENT_EXPORT(CREDENTIAL_MANAGEMENT) PasswordCredentialResponse {
 public:
  static jni_zero::ScopedJavaLocalRef<jobject> Create(
      JNIEnv* env,
      const PasswordCredentialResponse& credential);

  static PasswordCredentialResponse FromJavaPasswordCredentialResponse(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& j_credential);

  PasswordCredentialResponse(bool success,
                             std::u16string username,
                             std::u16string password);
  ~PasswordCredentialResponse() = default;
  PasswordCredentialResponse(const PasswordCredentialResponse&) = default;
  PasswordCredentialResponse(PasswordCredentialResponse&&) = default;
  PasswordCredentialResponse& operator=(const PasswordCredentialResponse&) =
      default;
  PasswordCredentialResponse& operator=(PasswordCredentialResponse&&) = default;

  bool success;
  std::u16string username;
  std::u16string password;
};

}  // namespace credential_management

namespace jni_zero {
template <>
inline credential_management::PasswordCredentialResponse
FromJniType<credential_management::PasswordCredentialResponse>(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_object) {
  return credential_management::PasswordCredentialResponse::
      FromJavaPasswordCredentialResponse(env, j_object);
}
template <>
inline jni_zero::ScopedJavaLocalRef<jobject>
ToJniType<credential_management::PasswordCredentialResponse>(
    JNIEnv* env,
    const credential_management::PasswordCredentialResponse& credential) {
  return credential_management::PasswordCredentialResponse::Create(env,
                                                                   credential);
}
}  // namespace jni_zero

#endif  // COMPONENTS_CREDENTIAL_MANAGEMENT_ANDROID_PASSWORD_CREDENTIAL_RESPONSE_H_
