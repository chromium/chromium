// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/credential_management/android/password_credential_response.h"

#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "components/credential_management/android/jni_headers/PasswordCredentialResponse_jni.h"

namespace credential_management {

PasswordCredentialResponse::PasswordCredentialResponse(bool success,
                                                       std::u16string username,
                                                       std::u16string password)
    : success(std::move(success)),
      username(std::move(username)),
      password(std::move(password)) {}

jni_zero::ScopedJavaLocalRef<jobject> PasswordCredentialResponse::Create(
    JNIEnv* env,
    const PasswordCredentialResponse& credential) {
  return Java_PasswordCredentialResponse_Constructor(
      env, credential.success, credential.username, credential.password);
}

PasswordCredentialResponse
PasswordCredentialResponse::FromJavaPasswordCredentialResponse(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& response) {
  return PasswordCredentialResponse(
      Java_PasswordCredentialResponse_getSuccess(env, response),
      Java_PasswordCredentialResponse_getUsername(env, response),
      Java_PasswordCredentialResponse_getPassword(env, response));
}

}  // namespace credential_management

DEFINE_JNI_FOR_PasswordCredentialResponse()
