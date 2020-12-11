// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOGIN_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOGIN_VIEW_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "components/password_manager/core/browser/http_auth_observer.h"
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
 public:
  // |login_model_data->model| is observed for the entire lifetime of the
  // LoginView. Therefore |login_model_data->model| should not be destroyed
  // before the LoginView object. |login_model_data| may be null.
  LoginView(const base::string16& authority,
            const base::string16& explanation,
            LoginHandler::LoginModelData* login_model_data);
  ~LoginView() override;

  // Access the data in the username/password text fields.
  const base::string16& GetUsername() const;
  const base::string16& GetPassword() const;

  // password_manager::HttpAuthObserver:
  void OnAutofillDataAvailable(const base::string16& username,
                               const base::string16& password) override;
  void OnLoginModelDestroying() override;

  // Used by LoginHandlerViews to set the initial focus.
  views::View* GetInitiallyFocusedView();

 private:
  // views::View:
  const char* GetClassName() const override;

  // Non-owning refs to the input text fields.
  views::Textfield* username_field_;
  views::Textfield* password_field_;

  // If not null, points to a model we need to notify of our own destruction
  // so it doesn't try and access this when its too late.
  password_manager::HttpAuthManager* http_auth_manager_;

  DISALLOW_COPY_AND_ASSIGN(LoginView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOGIN_VIEW_H_
