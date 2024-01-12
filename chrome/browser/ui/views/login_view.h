// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOGIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOGIN_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "components/password_manager/core/browser/http_auth_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Textfield;
}

namespace password_manager {
class HttpAuthManager;
}

// This class is responsible for displaying the contents of a login window
// for HTTP/FTP authentication.
class LoginView : public views::View,
                  public password_manager::HttpAuthObserver {
  METADATA_HEADER(LoginView, views::View)

 public:
  // |login_model_data->model| is observed for the entire lifetime of the
  // LoginView. Therefore |login_model_data->model| should not be destroyed
  // before the LoginView object. |login_model_data| may be null.
  LoginView(const std::u16string& authority,
            const std::u16string& explanation,
            LoginHandler::LoginModelData* login_model_data);
  LoginView(const LoginView&) = delete;
  LoginView& operator=(const LoginView&) = delete;
  ~LoginView() override;

  // Access the data in the username/password text fields.
  const std::u16string& GetUsername() const;
  const std::u16string& GetPassword() const;

  // password_manager::HttpAuthObserver:
  void OnAutofillDataAvailable(const std::u16string& username,
                               const std::u16string& password) override;
  void OnLoginModelDestroying() override;

  // Used by LoginHandlerViews to set the initial focus.
  views::View* GetInitiallyFocusedView();

 private:
  // Non-owning refs to the input text fields.
  raw_ptr<views::Textfield> username_field_;
  raw_ptr<views::Textfield> password_field_;

  // If not null, points to a model we need to notify of our own destruction
  // so it doesn't try and access this when its too late.
  raw_ptr<password_manager::HttpAuthManager> http_auth_manager_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOGIN_VIEW_H_
