// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_handler.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/chrome_http_auth_handler.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  void BuildViewImpl(const std::u16string& authority,
                     const std::u16string& explanation,
                     LoginModelData* login_model_data) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    content::WebContents* contents =
        web_contents()->GetResponsibleWebContents();
    CHECK(contents);

    if (vr::VrTabHelper::IsUiSuppressedInVr(
            contents, vr::UiSuppressedElement::kHttpAuth)) {
      CancelAuth();
      return;
    }

    TabAndroid* tab = TabAndroid::FromWebContents(contents);
    ui::ViewAndroid* view = contents->GetNativeView();
    ui::WindowAndroid* window = view ? view->GetWindowAndroid() : nullptr;
    // Notify WindowAndroid that HTTP authentication is required.
    if (tab && window) {
      chrome_http_auth_handler_ = std::make_unique<ChromeHttpAuthHandler>(
          authority, explanation, login_model_data);
      chrome_http_auth_handler_->Init();
      chrome_http_auth_handler_->SetObserver(this);
      chrome_http_auth_handler_->ShowDialog(tab->GetJavaObject(),
                                            window->GetJavaObject());
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
