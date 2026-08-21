// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_CONFIRM_PROTOCOL_HANDLER_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_CONFIRM_PROTOCOL_HANDLER_DIALOG_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "ui/base/interaction/element_identifier.h"

#if !BUILDFLAG(IS_ANDROID)

namespace content {
class WebContents;
}

namespace custom_handlers {
class ProtocolHandler;
}

namespace url {
class Origin;
}

namespace extensions {

DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kConfirmProtocolHandlerDialogHandlerRedirection);
DECLARE_ELEMENT_IDENTIFIER_VALUE(
    kConfirmProtocolHandlerDialogRememberMeCheckbox);

// Shows a dialog when the user tries to perform a navigation and the target url
// has a protocol handler registered by an extension to handle the url's scheme.
void ShowConfirmProtocolHandlerDialog(
    content::WebContents* web_contents,
    const custom_handlers::ProtocolHandler& handler,
    const std::optional<url::Origin>& initiating_origin,
    base::OnceCallback<void(bool)> granted_callback,
    base::OnceCallback<void()> denied_callback);

}  // namespace extensions

#endif  // !BUILDFLAG(IS_ANDROID)

#endif  // CHROME_BROWSER_UI_EXTENSIONS_CONFIRM_PROTOCOL_HANDLER_DIALOG_H_
