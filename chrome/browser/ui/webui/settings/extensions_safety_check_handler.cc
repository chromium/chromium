// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/extensions_safety_check_handler.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

ExtensionsSafetyCheckHandler::ExtensionsSafetyCheckHandler() = default;

ExtensionsSafetyCheckHandler::~ExtensionsSafetyCheckHandler() = default;

void ExtensionsSafetyCheckHandler::HandleGetExtensionsThatNeedReview(
    const base::Value::List& args) {
  const base::Value& callback_id = args[0];
  AllowJavascript();
  ResolveJavascriptCallback(callback_id,
                            base::Value(GetExtensionsThatNeedReview()));
}

std::u16string ExtensionsSafetyCheckHandler::GetExtensionsThatNeedReview() {
  // TODO(psarouthakis): Replace skeleton code to return real number of
  // extensions that need to be reviewed via the CWSInfoService and
  // update the string ID to use new extensions safety check strings.
  return (l10n_util::GetPluralStringFUTF16(
      IDS_SETTINGS_SAFETY_CHECK_EXTENSIONS_BLOCKLISTED_OFF, 1));
}

void ExtensionsSafetyCheckHandler::OnJavascriptAllowed() {}

void ExtensionsSafetyCheckHandler::OnJavascriptDisallowed() {}

void ExtensionsSafetyCheckHandler::RegisterMessages() {
  // Usage of base::Unretained(this) is safe, because web_ui() owns `this` and
  // won't release ownership until destruction.
  web_ui()->RegisterMessageCallback(
      "getExtensionsThatNeedReview",
      base::BindRepeating(
          &ExtensionsSafetyCheckHandler::HandleGetExtensionsThatNeedReview,
          base::Unretained(this)));
}

}  // namespace settings
