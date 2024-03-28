// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/dialogs/outdated_upgrade_bubble.h"

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/installer/util/google_update_util.h"
#endif

namespace {

// For ChromeOS Lacros, browser updates are done via system services, thus
// we redirect to the safetyCheck page that interacts with these. On other
// platforms it may be possible to download an updated browser via a site.
const char* kUpdateBrowserRedirectUrl =
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // The URL to be used to update Lacros-Chrome when auto-update failed
    // for too long.
    chrome::kChromeUIActivateSafetyCheckSettingsURL;
#else
    // The URL to be used to re-install Chrome when auto-update failed for
    // too long.
    "https://www.google.com/chrome";
#endif

bool g_upgrade_bubble_is_showing = false;

void OnWindowClosing() {
  g_upgrade_bubble_is_showing = false;
}

void OnDialogAccepted(content::PageNavigator* navigator,
                      bool auto_update_enabled,
                      const std::string& update_browser_redirect_url) {
  if (auto_update_enabled) {
    DCHECK(UpgradeDetector::GetInstance()->is_outdated_install());
    base::RecordAction(
        base::UserMetricsAction("OutdatedUpgradeBubble.Reinstall"));

    navigator->OpenURL(
        content::OpenURLParams(GURL(update_browser_redirect_url),
                               content::Referrer(),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui::PAGE_TRANSITION_LINK, false),
        /*navigation_handle_callback=*/{});
#if BUILDFLAG(IS_WIN)
  } else {
    DCHECK(UpgradeDetector::GetInstance()->is_outdated_install_no_au());
    base::RecordAction(
        base::UserMetricsAction("OutdatedUpgradeBubble.EnableAU"));
    // Record that the autoupdate flavour of the dialog has been shown.
    if (g_browser_process->local_state()) {
      g_browser_process->local_state()->SetBoolean(
          prefs::kAttemptedToEnableAutoupdate, true);
    }

    // Re-enable updates by shelling out to setup.exe asynchronously.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        base::BindOnce(&google_update::ElevateIfNeededToReenableUpdates));
#endif  // BUILDFLAG(IS_WIN)
  }
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
const char* GetUpdateUrlChannelSuffix(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::CANARY:
      return "/canary";
    case version_info::Channel::DEV:
      return "/dev";
    case version_info::Channel::BETA:
      return "/beta";
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::STABLE:
      return "";
  }
}
#endif

}  // namespace

void ShowOutdatedUpgradeBubble(Browser* browser, bool auto_update_enabled) {
  if (g_upgrade_bubble_is_showing)
    return;

  g_upgrade_bubble_is_showing = true;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto update_url = std::string(kUpdateBrowserRedirectUrl);
#else
  auto update_url = std::string(kUpdateBrowserRedirectUrl) +
                    GetUpdateUrlChannelSuffix(chrome::GetChannel());
#endif

  auto dialog_model =
      ui::DialogModel::Builder()
          .SetTitle(l10n_util::GetStringUTF16(IDS_UPGRADE_BUBBLE_TITLE))
          .AddOkButton(
              base::BindOnce(&OnDialogAccepted, browser, auto_update_enabled,
                             std::move(update_url)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(auto_update_enabled
                                                ? IDS_REINSTALL_APP
                                                : IDS_REENABLE_UPDATES)))
          .AddParagraph(
              ui::DialogModelLabel(IDS_UPGRADE_BUBBLE_TEXT).set_is_secondary())
          .SetDialogDestroyingCallback(base::BindOnce(&OnWindowClosing))
          .SetCloseActionCallback(base::BindOnce(
              &base::RecordAction,
              base::UserMetricsAction("OutdatedUpgradeBubble.Later")))
          .Build();

  chrome::ShowBubble(browser, kToolbarAppMenuButtonElementId,
                     std::move(dialog_model));

  base::RecordAction(
      auto_update_enabled
          ? base::UserMetricsAction("OutdatedUpgradeBubble.Show")
          : base::UserMetricsAction("OutdatedUpgradeBubble.ShowNoAU"));
}
