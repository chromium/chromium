// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_PARAMS_PROVIDERS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_PARAMS_PROVIDERS_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"

namespace content {
class WebContents;
}  // namespace content

namespace settings_overridden_params {

// Retrieves the params for displaying the NTP setting overridden dialog, if
// there is a controlling extension. Otherwise, returns an empty optional.
std::optional<ExtensionSettingsOverriddenDialog::Params> GetNtpOverriddenParams(
    Profile* profile);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Retrieves the params for displaying the dialog indicating that the default
// search engine has been overridden, if there is a controlling extension, and
// asynchronously passes them to the supplied callback. Otherwise, the callback
// is invoked with nullopt. Asynchronous operation allows fetching of
// extension-related resources such as icons.
void GetSearchOverriddenParamsThenRun(
    content::WebContents* web_contents,
    base::OnceCallback<
        void(std::unique_ptr<ExtensionSettingsOverriddenDialog::Params>)>
        done_callback);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace settings_overridden_params

#endif  // CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_OVERRIDDEN_PARAMS_PROVIDERS_H_
