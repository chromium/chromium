// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"

#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/controlled_home_bubble_delegate.h"
#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/extensions/settings_overridden_params_providers.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"

namespace extensions {

namespace {

// Whether the NTP post-install UI is enabled. By default, this is limited to
// Windows, Mac, and ChromeOS, but can be overridden for testing.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
bool g_ntp_post_install_ui_enabled = true;
#else
bool g_ntp_post_install_ui_enabled = false;
#endif

// Whether to acknowledge existing extensions overriding the NTP for the active
// profile. Active on MacOS to rollout the NTP bubble without prompting for
// previously-installed extensions.
// TODO(devlin): This has been rolled out on Mac for awhile; we can flip this to
// false (and keep the logic around for when/if we decide to expand the warning
// treatment to Linux).
bool g_acknowledge_existing_ntp_extensions =
#if BUILDFLAG(IS_MAC)
    true;
#else
    false;
#endif

// The name of the preference indicating whether existing NTP extensions have
// been automatically acknowledged.
const char kDidAcknowledgeExistingNtpExtensions[] =
    "ack_existing_ntp_extensions";

}  // namespace

// Whether a given ntp-overriding extension has been acknowledged by the user.
// The terse key value is because the pref has migrated between code layers.
const char kNtpOverridingExtensionAcknowledged[] = "ack_ntp_bubble";

void SetNtpPostInstallUiEnabledForTesting(bool enabled) {
  g_ntp_post_install_ui_enabled = enabled;
}

base::AutoReset<bool> SetAcknowledgeExistingNtpExtensionsForTesting(
    bool should_acknowledge) {
  return base::AutoReset<bool>(&g_acknowledge_existing_ntp_extensions,
                               should_acknowledge);
}

void AcknowledgePreExistingNtpExtensions(Profile* profile) {
  DCHECK(g_acknowledge_existing_ntp_extensions);

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  PrefService* profile_prefs = profile->GetPrefs();
  // Only acknowledge existing extensions once per profile.
  if (profile_prefs->GetBoolean(kDidAcknowledgeExistingNtpExtensions))
    return;

  profile_prefs->SetBoolean(kDidAcknowledgeExistingNtpExtensions, true);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile);
  for (const auto& extension : registry->enabled_extensions()) {
    const URLOverrides::URLOverrideMap& overrides =
        URLOverrides::GetChromeURLOverrides(extension.get());
    if (overrides.find(chrome::kChromeUINewTabHost) != overrides.end()) {
      prefs->UpdateExtensionPref(extension->id(),
                                 kNtpOverridingExtensionAcknowledged,
                                 base::Value(true));
    }
  }
}

void RegisterSettingsOverriddenUiPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kDidAcknowledgeExistingNtpExtensions, false,
                                PrefRegistry::NO_REGISTRATION_FLAGS);
}

void MaybeShowExtensionControlledHomeNotification(Browser* browser) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  auto bubble_delegate =
      std::make_unique<ControlledHomeBubbleDelegate>(browser);
  if (!bubble_delegate->ShouldShow()) {
    return;
  }

  bubble_delegate->PendingShow();
  browser->window()->GetExtensionsContainer()->ShowToolbarActionBubble(
      std::move(bubble_delegate));
#endif
}

void MaybeShowExtensionControlledSearchNotification(
    content::WebContents* web_contents,
    AutocompleteMatch::Type match_type) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (!AutocompleteMatch::IsSearchType(match_type) ||
      match_type == AutocompleteMatchType::SEARCH_OTHER_ENGINE) {
    return;
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser)
    return;

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetSearchOverriddenParams(browser->profile());
  if (!params)
    return;

  auto dialog = std::make_unique<ExtensionSettingsOverriddenDialog>(
      std::move(*params), browser->profile());
  if (!dialog->ShouldShow())
    return;

  ShowSettingsOverriddenDialog(std::move(dialog), browser);
#endif
}

void MaybeShowExtensionControlledNewTabPage(
    Browser* browser, content::WebContents* web_contents) {
  if (!g_ntp_post_install_ui_enabled)
    return;

  // Acknowledge existing extensions if necessary.
  if (g_acknowledge_existing_ntp_extensions)
    AcknowledgePreExistingNtpExtensions(browser->profile());

  // Jump through a series of hoops to see if the web contents is pointing to
  // an extension-controlled NTP.
  // TODO(devlin): Some of this is redundant with the checks in the bubble/
  // dialog. We should consolidate, but that'll be simpler once we only have
  // one UI option. In the meantime, extra checks don't hurt.
  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  if (!entry)
    return;
  GURL active_url = entry->GetURL();
  if (!active_url.SchemeIs(extensions::kExtensionScheme))
    return;  // Not a URL that we care about.

  // See if the current active URL matches a transformed NewTab URL.
  GURL ntp_url(chrome::kChromeUINewTabURL);
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &ntp_url, web_contents->GetBrowserContext());
  if (ntp_url != active_url)
    return;  // Not being overridden by an extension.

  Profile* const profile = browser->profile();

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetNtpOverriddenParams(profile);
  if (!params)
    return;

  auto dialog = std::make_unique<ExtensionSettingsOverriddenDialog>(
      std::move(*params), profile);
  if (!dialog->ShouldShow())
    return;

  ShowSettingsOverriddenDialog(std::move(dialog), browser);
}

}  // namespace extensions
