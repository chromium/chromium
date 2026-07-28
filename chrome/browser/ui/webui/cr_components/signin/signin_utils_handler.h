// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SIGNIN_SIGNIN_UTILS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SIGNIN_SIGNIN_UTILS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/resources/cr_components/signin/signin.mojom.h"

class Profile;

class SigninUtilsHandler : public signin::mojom::SigninPageHandler {
 public:
  SigninUtilsHandler(
      mojo::PendingReceiver<signin::mojom::SigninPageHandler> receiver,
      Profile* profile);

  SigninUtilsHandler(const SigninUtilsHandler&) = delete;
  SigninUtilsHandler& operator=(const SigninUtilsHandler&) = delete;

  ~SigninUtilsHandler() override;

  // signin::mojom::SigninPageHandler:
  void StartSignin(
      signin::mojom::ChromeSigninAccessPoint access_point) override;
  void SigninWithAccount(signin::mojom::ChromeSigninAccessPoint access_point,
                         const std::string& email,
                         bool is_default_promo_account) override;
  void RecordSigninPendingOffered() override;
  void RecordSigninOffered(
      signin::mojom::ChromeSigninAccessPoint access_point) override;

 private:
  mojo::Receiver<signin::mojom::SigninPageHandler> receiver_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SIGNIN_SIGNIN_UTILS_HANDLER_H_
