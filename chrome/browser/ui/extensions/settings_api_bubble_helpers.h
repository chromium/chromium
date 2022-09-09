// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_API_BUBBLE_HELPERS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_API_BUBBLE_HELPERS_H_

#include "base/auto_reset.h"
#include "components/omnibox/browser/autocomplete_match.h"

class Browser;
class PrefRegistrySimple;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {

extern const char kNtpOverridingExtensionAcknowledged[];

// Sets whether the NTP post-install UI is enabled for testing purposes.
// TODO(devlin): This would be cooler as a base::AutoReset<>.
void SetNtpPostInstallUiEnabledForTesting(bool enabled);

// Testing-only method to configure if existing NTP extensions are
// auto-acknowledged.
base::AutoReset<bool> SetAcknowledgeExistingNtpExtensionsForTesting(
    bool should_acknowledge);

// Registers prefs related to the settings overridden UI.
void RegisterSettingsOverriddenUiPrefs(PrefRegistrySimple* registry);

// Iterates over existing NTP-overriding extensions installed in the given
// `profile` and marks them as acknowledged. Stores a preference indicating
// the action was completed. Subsequent calls will *not* acknowledge more
// extensions. This is needed to avoid prompting users with existing
// extensions when we expand the warning to new platforms.
void AcknowledgePreExistingNtpExtensions(Profile* profile);

// Shows a bubble notifying the user that the homepage is controlled by an
// extension. This bubble is shown only on the first use of the Home button
// after the controlling extension takes effect.
void MaybeShowExtensionControlledHomeNotification(Browser* browser);

// Shows a bubble notifying the user that the search engine is controlled by an
// extension. This bubble is shown only on the first search after the
// controlling extension takes effect.
void MaybeShowExtensionControlledSearchNotification(
    content::WebContents* web_contents,
    AutocompleteMatch::Type match_type);

// Shows a bubble notifying the user that the new tab page is controlled by an
// extension. This bubble is shown only the first time the new tab page is shown
// after the controlling extension takes effect.
void MaybeShowExtensionControlledNewTabPage(
    Browser* browser,
    content::WebContents* web_contents);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_SETTINGS_API_BUBBLE_HELPERS_H_
