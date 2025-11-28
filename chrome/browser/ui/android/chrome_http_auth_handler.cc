// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/chrome_http_auth_handler.h"

#include <jni.h>

#include <string>
#include <string_view>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ChromeHttpAuthHandler_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

ChromeHttpAuthHandler::ChromeHttpAuthHandler(
    const std::u16string& authority,
    const std::u16string& explanation,
    LoginHandler::LoginModelData* login_model_data)
    : observer_(nullptr),
      authority_(authority),
      explanation_(explanation),
      auth_manager_(login_model_data ? login_model_data->model.get()
                                     : nullptr) {
  if (login_model_data) {
    auth_manager_->SetObserverAndDeliverCredentials(this,
                                                    *login_model_data->form);
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

void ChromeHttpAuthHandler::Init(LoginHandler* observer) {
  observer_ = observer;

  DCHECK(java_chrome_http_auth_handler_.is_null());
  JNIEnv* env = AttachCurrentThread();
  java_chrome_http_auth_handler_.Reset(
      Java_ChromeHttpAuthHandler_create(env, reinterpret_cast<intptr_t>(this)));
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
    std::u16string_view username,
    std::u16string_view password) {
  DCHECK(java_chrome_http_auth_handler_.obj());
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ChromeHttpAuthHandler_onAutofillDataAvailable(
      env, java_chrome_http_auth_handler_, std::u16string(username),
      std::u16string(password));
}

void ChromeHttpAuthHandler::OnLoginModelDestroying() {
  auth_manager_->DetachObserver(this);
  auth_manager_ = nullptr;
}

void ChromeHttpAuthHandler::SetAuth(JNIEnv* env,
                                    std::u16string& username,
                                    std::u16string& password) {
  // SetAuthSync can result in destruction of `this`. We post task to make
  // destruction asynchronous and avoid re-entrancy.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromeHttpAuthHandler::SetAuthSync,
                     weak_factory_.GetWeakPtr(), username, password));
}

void ChromeHttpAuthHandler::CancelAuth(JNIEnv* env) {
  // CancelAuthSync can result in destruction of `this`. We post task to make
  // destruction asynchronous and avoid re-entrancy.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ChromeHttpAuthHandler::CancelAuthSync,
                                weak_factory_.GetWeakPtr()));
}

std::u16string ChromeHttpAuthHandler::GetMessageBody(JNIEnv* env) {
  if (explanation_.empty()) {
    return authority_;
  }
  return authority_ + u" " + explanation_;
}

void ChromeHttpAuthHandler::SetAuthSync(const std::u16string& username,
                                        const std::u16string& password) {
  observer_->SetAuth(username, password);
}

void ChromeHttpAuthHandler::CancelAuthSync() {
  observer_->CancelAuth(/*notify_others=*/true);
}

DEFINE_JNI(ChromeHttpAuthHandler)
