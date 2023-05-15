// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_extensions_handler.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

SafetyCheckExtensionsHandler::SafetyCheckExtensionsHandler() = default;

SafetyCheckExtensionsHandler::~SafetyCheckExtensionsHandler() = default;

void SafetyCheckExtensionsHandler::HandleGetNumberOfExtensionsThatNeedReview(
    const base::Value::List& args) {
  const base::Value& callback_id = args[0];
  AllowJavascript();
  ResolveJavascriptCallback(callback_id,
                            base::Value(GetNumberOfExtensionsThatNeedReview()));
}

int SafetyCheckExtensionsHandler::GetNumberOfExtensionsThatNeedReview() {
  // TODO(psarouthakis): Replace skeleton code to return real number of
  // extensions that need to be reviewed via the CWSInfoService.
  return 2;
}

void SafetyCheckExtensionsHandler::OnJavascriptAllowed() {}

void SafetyCheckExtensionsHandler::OnJavascriptDisallowed() {}

void SafetyCheckExtensionsHandler::RegisterMessages() {
  // Usage of base::Unretained(this) is safe, because web_ui() owns `this` and
  // won't release ownership until destruction.
  web_ui()->RegisterMessageCallback(
      "getNumberOfExtensionsThatNeedReview",
      base::BindRepeating(&SafetyCheckExtensionsHandler::
                              HandleGetNumberOfExtensionsThatNeedReview,
                          base::Unretained(this)));
}

}  // namespace settings
