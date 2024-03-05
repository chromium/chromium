// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/signin_fatal_error_screen_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/signin_fatal_error_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"

namespace ash {

SignInFatalErrorScreenHandler::SignInFatalErrorScreenHandler()
    : BaseScreenHandler(kScreenId) {}

SignInFatalErrorScreenHandler::~SignInFatalErrorScreenHandler() = default;

void SignInFatalErrorScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("errorGenericFatalErrorTitle",
               IDS_OOBE_GENERIC_FATAL_ERROR_TITLE);
  builder->Add("fatalErrorMessageNoPassword",
               IDS_LOGIN_FATAL_ERROR_NO_PASSWORD);
  builder->Add("fatalErrorMessageVerificationFailed",
               IDS_LOGIN_FATAL_ERROR_PASSWORD_VERIFICATION);
  builder->Add("fatalErrorTryAgainButton",
               IDS_LOGIN_FATAL_ERROR_TRY_AGAIN_BUTTON);
  builder->Add("fatalErrorDoneButton", IDS_DONE);
  builder->Add("fatalErrorMessageNoAccountDetails",
               IDS_LOGIN_FATAL_ERROR_NO_ACCOUNT_DETAILS);
  builder->Add("fatalErrorMessageInsecureURL",
               IDS_LOGIN_FATAL_ERROR_TEXT_INSECURE_URL);
}

void SignInFatalErrorScreenHandler::Show(SignInFatalErrorScreen::Error error,
                                         const base::Value::Dict& params) {
  base::Value::Dict screen_data = params.Clone();
  screen_data.Set("errorState", base::Value(static_cast<int>(error)));

  ShowInWebUI(std::move(screen_data));
}

base::WeakPtr<SignInFatalErrorView> SignInFatalErrorScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
