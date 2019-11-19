// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_handler.h"

#include <memory>

#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/chrome_http_auth_handler.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using content::BrowserThread;
using net::AuthChallengeInfo;

namespace {

class LoginHandlerAndroid : public LoginHandler {
 public:
  LoginHandlerAndroid(const net::AuthChallengeInfo& auth_info,
                      content::WebContents* web_contents,
                      LoginAuthRequiredCallback auth_required_callback)
      : LoginHandler(auth_info,
                     web_contents,
                     std::move(auth_required_callback)) {}

  ~LoginHandlerAndroid() override {
    // LoginHandler cannot call CloseDialog because the subclass will already
    // have been destructed.
    CloseDialog();
  }

 protected:
  // LoginHandler methods:
  void BuildViewImpl(const base::string16& authority,
                     const base::string16& explanation,
                     LoginModelData* login_model_data) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // Get pointer to TabAndroid
    CHECK(web_contents());
    ViewAndroidHelper* view_helper =
        ViewAndroidHelper::FromWebContents(web_contents());

    if (vr::VrTabHelper::IsUiSuppressedInVr(
            web_contents(), vr::UiSuppressedElement::kHttpAuth)) {
      CancelAuth();
      return;
    }

    TabAndroid* tab = TabAndroid::FromWebContents(web_contents());
    // Notify WindowAndroid that HTTP authentication is required.
    if (tab && view_helper->GetViewAndroid() &&
        view_helper->GetViewAndroid()->GetWindowAndroid()) {
      chrome_http_auth_handler_.reset(
          new ChromeHttpAuthHandler(authority, explanation, login_model_data));
      chrome_http_auth_handler_->Init();
      chrome_http_auth_handler_->SetObserver(this);
      chrome_http_auth_handler_->ShowDialog(
          tab->GetJavaObject(),
          view_helper->GetViewAndroid()->GetWindowAndroid()->GetJavaObject());
    } else {
      CancelAuth();
      LOG(WARNING) << "HTTP Authentication failed because TabAndroid is "
          "missing";
    }
  }

  void CloseDialog() override {
    if (chrome_http_auth_handler_)
      chrome_http_auth_handler_->CloseDialog();
  }

 private:
  std::unique_ptr<ChromeHttpAuthHandler> chrome_http_auth_handler_;
};

}  // namespace

// static
std::unique_ptr<LoginHandler> LoginHandler::Create(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    LoginAuthRequiredCallback auth_required_callback) {
  return std::make_unique<LoginHandlerAndroid>(
      auth_info, web_contents, std::move(auth_required_callback));
}
