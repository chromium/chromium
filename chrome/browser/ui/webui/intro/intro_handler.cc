// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/intro_handler.h"

IntroHandler::IntroHandler(base::RepeatingCallback<void(bool sign_in)> callback)
    : callback_(std::move(callback)) {
  DCHECK(callback_);
}

IntroHandler::~IntroHandler() = default;

void IntroHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "continueWithoutAccount",
      base::BindRepeating(&IntroHandler::HandleContinueWithoutAccount,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "continueWithAccount",
      base::BindRepeating(&IntroHandler::HandleContinueWithAccount,
                          base::Unretained(this)));
}

void IntroHandler::HandleContinueWithAccount(const base::Value::List& args) {
  CHECK(args.empty());
  callback_.Run(/*sign_in=*/true);
}

void IntroHandler::HandleContinueWithoutAccount(const base::Value::List& args) {
  CHECK(args.empty());
  callback_.Run(/*sign_in=*/false);
}
