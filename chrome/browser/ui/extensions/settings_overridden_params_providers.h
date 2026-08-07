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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(MissingParams)
enum class MissingParams {
  kNone = 0,
  kMissingNewSearchName = 1,
  kMissingPreviousSearchName = 2,
  kMaxValue = kMissingPreviousSearchName,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/extensions/enums.xml:MissingParams)

// Retrieves the params for displaying the NTP setting overridden dialog, if
// there is a controlling extension. Otherwise, returns an empty optional.
std::optional<ExtensionSettingsOverriddenDialog::Params> GetNtpOverriddenParams(
    Profile* profile);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Returns true if the extension controlling the default search engine set the
// *same* engine the user was already using, in which case nothing was actually
// overridden and there is nothing to confirm. This happens when a user selects
// a search engine and then installs an extension that provides the same one.
//
// This is synchronous and must stay that way: callers use it to decide whether
// a navigation needs to be held for confirmation, and that decision has to be
// complete before the navigation is abandoned. See
// https://crbug.com/540532980 for what happens when it is not.
bool ExtensionSearchOverrideMatchesExistingEngine(Profile* profile);

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
