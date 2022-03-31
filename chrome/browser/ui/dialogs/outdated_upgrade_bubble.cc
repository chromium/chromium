// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/dialogs/outdated_upgrade_bubble.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
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

// The maximum number of ignored bubble we track in the NumLaterPerReinstall
// histogram.
constexpr int kMaxIgnored = 50;
// The number of buckets we want the NumLaterPerReinstall histogram to use.
constexpr int kNumIgnoredBuckets = 5;

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

// The number of times the user ignored the bubble before finally choosing to
// reinstall.
int g_num_ignored_bubbles = 0;

void OnWindowClosing() {
  g_upgrade_bubble_is_showing = false;

  // Increment the ignored bubble count (if this bubble wasn't ignored, this
  // increment is offset by a decrement in OnDialogAccepted()).
  if (g_num_ignored_bubbles < kMaxIgnored)
    ++g_num_ignored_bubbles;
}

void OnDialogAccepted(content::PageNavigator* navigator,
                      bool auto_update_enabled,
                      const char* update_browser_redirect_url) {
  // Offset the +1 in OnWindowClosing().
  --g_num_ignored_bubbles;
  if (auto_update_enabled) {
    DCHECK(UpgradeDetector::GetInstance()->is_outdated_install());
    UMA_HISTOGRAM_CUSTOM_COUNTS("OutdatedUpgradeBubble.NumLaterPerReinstall",
                                g_num_ignored_bubbles, 1, kMaxIgnored,
                                kNumIgnoredBuckets);
    base::RecordAction(
        base::UserMetricsAction("OutdatedUpgradeBubble.Reinstall"));

    navigator->OpenURL(content::OpenURLParams(
        GURL(update_browser_redirect_url), content::Referrer(),
        WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
        false));
#if BUILDFLAG(IS_WIN)
  } else {
    DCHECK(UpgradeDetector::GetInstance()->is_outdated_install_no_au());
    UMA_HISTOGRAM_CUSTOM_COUNTS("OutdatedUpgradeBubble.NumLaterPerEnableAU",
                                g_num_ignored_bubbles, 1, kMaxIgnored,
                                kNumIgnoredBuckets);
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

}  // namespace

void ShowOutdatedUpgradeBubble(Browser* browser, bool auto_update_enabled) {
  if (g_upgrade_bubble_is_showing)
    return;

  g_upgrade_bubble_is_showing = true;

  auto dialog_model =
      ui::DialogModel::Builder()
          .SetTitle(l10n_util::GetStringUTF16(IDS_UPGRADE_BUBBLE_TITLE))
          .AddOkButton(
              base::BindOnce(&OnDialogAccepted, browser, auto_update_enabled,
                             kUpdateBrowserRedirectUrl),
              l10n_util::GetStringUTF16(auto_update_enabled
                                            ? IDS_REINSTALL_APP
                                            : IDS_REENABLE_UPDATES))
          .AddBodyText(
              ui::DialogModelLabel(IDS_UPGRADE_BUBBLE_TEXT).set_is_secondary())
          .SetDialogDestroyingCallback(base::BindOnce(&OnWindowClosing))
          .SetCloseActionCallback(base::BindOnce(
              &base::RecordAction,
              base::UserMetricsAction("OutdatedUpgradeBubble.Later")))
          .Build();

  chrome::ShowBubble(browser, kAppMenuButtonElementId, std::move(dialog_model));

  chrome::RecordDialogCreation(chrome::DialogIdentifier::OUTDATED_UPGRADE);

  base::RecordAction(
      auto_update_enabled
          ? base::UserMetricsAction("OutdatedUpgradeBubble.Show")
          : base::UserMetricsAction("OutdatedUpgradeBubble.ShowNoAU"));
}
