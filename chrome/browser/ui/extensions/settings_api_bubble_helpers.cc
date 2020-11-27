// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"

#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/ntp_overridden_bubble_delegate.h"
#include "chrome/browser/extensions/settings_api_bubble_delegate.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_bridge.h"
#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"
#include "chrome/browser/ui/extensions/settings_overridden_params_providers.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/common/constants.h"

namespace extensions {

namespace {

// Whether the NTP post-install UI is enabled. By default, this is limited to
// Windows, Mac, and ChromeOS, but can be overridden for testing.
#if defined(OS_WIN) || defined(OS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
bool g_ntp_post_install_ui_enabled = true;
#else
bool g_ntp_post_install_ui_enabled = false;
#endif

#if defined(OS_WIN) || defined(OS_MAC)
void ShowSettingsApiBubble(SettingsApiOverrideType type,
                           Browser* browser) {
  ToolbarActionsModel* model = ToolbarActionsModel::Get(browser->profile());
  if (model->has_active_bubble())
    return;

  std::unique_ptr<ExtensionMessageBubbleController> settings_api_bubble(
      new ExtensionMessageBubbleController(
          new SettingsApiBubbleDelegate(browser->profile(), type), browser));
  if (!settings_api_bubble->ShouldShow())
    return;

  settings_api_bubble->SetIsActiveBubble();
  std::unique_ptr<ToolbarActionsBarBubbleDelegate> bridge(
      new ExtensionMessageBubbleBridge(std::move(settings_api_bubble)));
  browser->window()->GetExtensionsContainer()->ShowToolbarActionBubbleAsync(
      std::move(bridge));
}
#endif

}  // namespace

void SetNtpPostInstallUiEnabledForTesting(bool enabled) {
  g_ntp_post_install_ui_enabled = enabled;
}

void MaybeShowExtensionControlledHomeNotification(Browser* browser) {
#if defined(OS_WIN) || defined(OS_MAC)
  ShowSettingsApiBubble(BUBBLE_TYPE_HOME_PAGE, browser);
#endif
}

void MaybeShowExtensionControlledSearchNotification(
    content::WebContents* web_contents,
    AutocompleteMatch::Type match_type) {
#if defined(OS_WIN) || defined(OS_MAC)
  if (!AutocompleteMatch::IsSearchType(match_type) ||
      match_type == AutocompleteMatchType::SEARCH_OTHER_ENGINE) {
    return;
  }

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  if (base::FeatureList::IsEnabled(
          features::kExtensionSettingsOverriddenDialogs)) {
    base::Optional<ExtensionSettingsOverriddenDialog::Params> params =
        settings_overridden_params::GetSearchOverriddenParams(
            browser->profile());
    if (!params)
      return;

    auto dialog = std::make_unique<ExtensionSettingsOverriddenDialog>(
        std::move(*params), browser->profile());
    if (!dialog->ShouldShow())
      return;

    chrome::ShowExtensionSettingsOverriddenDialog(std::move(dialog), browser);
  } else {
    ShowSettingsApiBubble(BUBBLE_TYPE_SEARCH_ENGINE, browser);
  }
#endif
}

void MaybeShowExtensionControlledNewTabPage(
    Browser* browser, content::WebContents* web_contents) {
  if (!g_ntp_post_install_ui_enabled)
    return;

  // Acknowledge existing extensions if necessary.
  NtpOverriddenBubbleDelegate::MaybeAcknowledgeExistingNtpExtensions(
      browser->profile());

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
  ToolbarActionsModel* model = ToolbarActionsModel::Get(profile);
  if (model->has_active_bubble())
    return;

  if (base::FeatureList::IsEnabled(
          features::kExtensionSettingsOverriddenDialogs)) {
    base::Optional<ExtensionSettingsOverriddenDialog::Params> params =
        settings_overridden_params::GetNtpOverriddenParams(profile);
    if (!params)
      return;

    auto dialog = std::make_unique<ExtensionSettingsOverriddenDialog>(
        std::move(*params), profile);
    if (!dialog->ShouldShow())
      return;

    chrome::ShowExtensionSettingsOverriddenDialog(std::move(dialog), browser);
    return;
  }

  std::unique_ptr<ExtensionMessageBubbleController> ntp_overridden_bubble(
      new ExtensionMessageBubbleController(
          new NtpOverriddenBubbleDelegate(profile), browser));
  if (!ntp_overridden_bubble->ShouldShow())
    return;

  ntp_overridden_bubble->SetIsActiveBubble();
  std::unique_ptr<ToolbarActionsBarBubbleDelegate> bridge(
      new ExtensionMessageBubbleBridge(std::move(ntp_overridden_bubble)));
  browser->window()->GetExtensionsContainer()->ShowToolbarActionBubbleAsync(
      std::move(bridge));
}

}  // namespace extensions
