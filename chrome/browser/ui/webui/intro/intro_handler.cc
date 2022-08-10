// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/intro_handler.h"

#include "base/logging.h"

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
  DVLOG(1) << "HandleContinueWithAccount - To be implemented.";
}

void IntroHandler::HandleContinueWithoutAccount(const base::Value::List& args) {
  CHECK(args.empty());
  DVLOG(1) << "HandleContinueWithoutAccount - To be implemented.";
}
