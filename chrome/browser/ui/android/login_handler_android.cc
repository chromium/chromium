// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_handler.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/chrome_http_auth_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/display/types/display_constants.h"
#include "url/gurl.h"

using content::BrowserThread;
using net::AuthChallengeInfo;

namespace {

class LoginHandlerAndroid : public LoginHandler {
 public:
  LoginHandlerAndroid(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback)
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
  bool BuildViewImpl(const std::u16string& authority,
                     const std::u16string& explanation,
                     LoginModelData* login_model_data) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    content::WebContents* contents =
        web_contents() ? web_contents()->GetResponsibleWebContents() : nullptr;
    if (!contents) {
      return false;
    }

    TabAndroid* tab = TabAndroid::FromWebContents(contents);
    ui::ViewAndroid* view = contents->GetNativeView();
    ui::WindowAndroid* window = view ? view->GetWindowAndroid() : nullptr;
    // Notify WindowAndroid that HTTP authentication is required.
    if (tab && window) {
      base::WeakPtr<LoginHandler> weak_this = GetWeakPtr();
      auto blocker = contents->ForSecurityDropFullscreen(
          /*display_id=*/display::kInvalidDisplayId);
      if (!weak_this || !blocker) {
        return false;
      }
      contents = web_contents() ? web_contents()->GetResponsibleWebContents()
                                : nullptr;
      if (!contents) {
        return false;
      }
      tab = TabAndroid::FromWebContents(contents);
      view = contents->GetNativeView();
      window = view ? view->GetWindowAndroid() : nullptr;
      if (!tab || !window) {
        return false;
      }

      fullscreen_blocker_ = std::move(*blocker);

      chrome_http_auth_handler_ = std::make_unique<ChromeHttpAuthHandler>(
          authority, explanation, auth_info().challenger.GetURL(),
          login_model_data);
      chrome_http_auth_handler_->Init(this);
      chrome_http_auth_handler_->ShowDialog(tab->GetJavaObject(),
                                            window->GetJavaObject());
      return true;
    } else {
      LOG(WARNING) << "HTTP Authentication failed because TabAndroid is "
                      "missing";
      return false;
    }
  }

  void CloseDialog() override {
    fullscreen_blocker_.RunAndReset();
    if (chrome_http_auth_handler_) {
      chrome_http_auth_handler_->CloseDialog();
    }
  }

 private:
  base::ScopedClosureRunner fullscreen_blocker_;
  std::unique_ptr<ChromeHttpAuthHandler> chrome_http_auth_handler_;
};

}  // namespace

// static
std::unique_ptr<LoginHandler> LoginHandler::Create(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    content::LoginDelegate::LoginAuthRequiredCallback auth_required_callback) {
  return std::make_unique<LoginHandlerAndroid>(
      auth_info, web_contents, std::move(auth_required_callback));
}
