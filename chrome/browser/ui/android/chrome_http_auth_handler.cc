// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/chrome_http_auth_handler.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/ChromeHttpAuthHandler_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

ChromeHttpAuthHandler::ChromeHttpAuthHandler(
    const base::string16& authority,
    const base::string16& explanation,
    LoginHandler::LoginModelData* login_model_data)
    : observer_(nullptr),
      authority_(authority),
      explanation_(explanation),
      auth_manager_(login_model_data ? login_model_data->model : nullptr) {
  if (login_model_data) {
    auth_manager_->SetObserverAndDeliverCredentials(this,
                                                    login_model_data->form);
  }
}

ChromeHttpAuthHandler::~ChromeHttpAuthHandler() {
  if (auth_manager_) {
    auth_manager_->DetachObserver(this);
  }
  if (java_chrome_http_auth_handler_) {
    JNIEnv* env = AttachCurrentThread();
    Java_ChromeHttpAuthHandler_onNativeDestroyed(
        env, java_chrome_http_auth_handler_);
  }
}

void ChromeHttpAuthHandler::Init() {
  DCHECK(java_chrome_http_auth_handler_.is_null());
  JNIEnv* env = AttachCurrentThread();
  java_chrome_http_auth_handler_.Reset(
      Java_ChromeHttpAuthHandler_create(env, reinterpret_cast<intptr_t>(this)));
}

void ChromeHttpAuthHandler::SetObserver(LoginHandler* observer) {
  observer_ = observer;
}

void ChromeHttpAuthHandler::ShowDialog(const JavaRef<jobject>& tab_android,
                                       const JavaRef<jobject>& window_android) {
  JNIEnv* env = AttachCurrentThread();
  Java_ChromeHttpAuthHandler_showDialog(env, java_chrome_http_auth_handler_,
                                        tab_android, window_android);
}

void ChromeHttpAuthHandler::CloseDialog() {
  JNIEnv* env = AttachCurrentThread();
  Java_ChromeHttpAuthHandler_closeDialog(env, java_chrome_http_auth_handler_);
}

void ChromeHttpAuthHandler::OnAutofillDataAvailable(
    const base::string16& username,
    const base::string16& password) {
  DCHECK(java_chrome_http_auth_handler_.obj() != NULL);
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_username =
      ConvertUTF16ToJavaString(env, username);
  ScopedJavaLocalRef<jstring> j_password =
      ConvertUTF16ToJavaString(env, password);
  Java_ChromeHttpAuthHandler_onAutofillDataAvailable(
      env, java_chrome_http_auth_handler_, j_username, j_password);
}

void ChromeHttpAuthHandler::OnLoginModelDestroying() {
  auth_manager_->DetachObserver(this);
  auth_manager_ = nullptr;
}

void ChromeHttpAuthHandler::SetAuth(JNIEnv* env,
                                    const JavaParamRef<jobject>&,
                                    const JavaParamRef<jstring>& username,
                                    const JavaParamRef<jstring>& password) {
  if (observer_) {
    base::string16 username16 = ConvertJavaStringToUTF16(env, username);
    base::string16 password16 = ConvertJavaStringToUTF16(env, password);
    observer_->SetAuth(username16, password16);
  }
}

void ChromeHttpAuthHandler::CancelAuth(JNIEnv* env,
                                       const JavaParamRef<jobject>&) {
  if (observer_)
    observer_->CancelAuth();
}

ScopedJavaLocalRef<jstring> ChromeHttpAuthHandler::GetMessageBody(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  if (explanation_.empty())
    return ConvertUTF16ToJavaString(env, authority_);
  return ConvertUTF16ToJavaString(
      env, authority_ + base::ASCIIToUTF16(" ") + explanation_);
}
