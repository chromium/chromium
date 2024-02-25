// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_CHROME_HTTP_AUTH_HANDLER_H_
#define CHROME_BROWSER_UI_ANDROID_CHROME_HTTP_AUTH_HANDLER_H_

#include <jni.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "components/password_manager/core/browser/http_auth_observer.h"

namespace password_manager {
class HttpAuthManager;
}

// This class facilitates communication between a native LoginHandler
// and a Java land ChromeHttpAuthHandler, which is passed to a
// ContentViewClient to allow it to respond to HTTP authentication requests
// by, e.g., showing the user a login dialog.
class ChromeHttpAuthHandler : public password_manager::HttpAuthObserver {
 public:
  ChromeHttpAuthHandler(const std::u16string& authority,
                        const std::u16string& explanation,
                        LoginHandler::LoginModelData* login_model_data);

  ChromeHttpAuthHandler(const ChromeHttpAuthHandler&) = delete;
  ChromeHttpAuthHandler& operator=(const ChromeHttpAuthHandler&) = delete;

  ~ChromeHttpAuthHandler() override;

  // This must be called before using the object.
  // Constructs a corresponding Java land ChromeHttpAuthHandler.
  // `observer` is forwarded callbacks from SetAuth() and CancelAuth().
  void Init(LoginHandler* observer);

  // Show the dialog prompting for login credentials.
  void ShowDialog(const base::android::JavaRef<jobject>& tab_android,
                  const base::android::JavaRef<jobject>& window_android);

  // Close the dialog if showing.
  void CloseDialog();

  // password_manager::HttpAuthObserver:
  void OnAutofillDataAvailable(const std::u16string& username,
                               const std::u16string& password) override;
  void OnLoginModelDestroying() override;

  // --------------------------------------------------------------
  // JNI Methods
  // --------------------------------------------------------------

  // Submits the username and password to the observer.
  void SetAuth(JNIEnv* env,
               const base::android::JavaParamRef<jobject>&,
               const base::android::JavaParamRef<jstring>& username,
               const base::android::JavaParamRef<jstring>& password);

  // Cancels the authentication attempt of the observer.
  void CancelAuth(JNIEnv* env, const base::android::JavaParamRef<jobject>&);

  // These functions return the strings needed to display a login form.
  base::android::ScopedJavaLocalRef<jstring> GetMessageBody(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&);

 private:
  void SetAuthSync(const std::u16string& username,
                   const std::u16string& password);
  void CancelAuthSync();

  // Owns this class and is guaranteed to outlive it.
  raw_ptr<LoginHandler> observer_;

  base::android::ScopedJavaGlobalRef<jobject> java_chrome_http_auth_handler_;
  std::u16string authority_;
  std::u16string explanation_;

  // If not null, points to a model we need to notify of our own destruction
  // so it doesn't try and access this when its too late.
  raw_ptr<password_manager::HttpAuthManager> auth_manager_;

  base::WeakPtrFactory<ChromeHttpAuthHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ANDROID_CHROME_HTTP_AUTH_HANDLER_H_
