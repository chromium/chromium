// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater_win.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "base/win/win_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"

VersionUpdaterWin::VersionUpdaterWin(gfx::AcceleratedWidget owner_widget)
    : owner_widget_(owner_widget), weak_factory_(this) {
}

VersionUpdaterWin::~VersionUpdaterWin() {
}

void VersionUpdaterWin::CheckForUpdate(const StatusCallback& callback,
                                       const PromoteCallback&) {
  // There is no supported integration with Google Update for Chromium.
  callback_ = callback;

  callback_.Run(CHECKING, 0, false, std::string(), 0, base::string16());
  DoBeginUpdateCheck(false /* !install_update_if_possible */);
}

void VersionUpdaterWin::OnUpdateCheckComplete(
    const base::string16& new_version) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (new_version.empty()) {
    // Google Update says that no new version is available. Check to see if a
    // restart is needed for a previously-applied update to take effect.
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_VISIBLE},
        base::Bind(&upgrade_util::IsUpdatePendingRestart),
        base::Bind(&VersionUpdaterWin::OnPendingRestartCheck,
                   weak_factory_.GetWeakPtr()));
    // Early exit since callback_ will be Run in OnPendingRestartCheck.
    return;
  }

  // Notify the caller that the update is now beginning and initiate it.
  DoBeginUpdateCheck(true /* install_update_if_possible */);
  callback_.Run(UPDATING, 0, false, std::string(), 0, base::string16());
}

void VersionUpdaterWin::OnUpgradeProgress(int progress,
                                          const base::string16& new_version) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callback_.Run(UPDATING, progress, false, std::string(), 0, base::string16());
}

void VersionUpdaterWin::OnUpgradeComplete(const base::string16& new_version) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callback_.Run(NEARLY_UPDATED, 0, false, std::string(), 0, base::string16());
}

void VersionUpdaterWin::OnError(GoogleUpdateErrorCode error_code,
                                const base::string16& html_error_message,
                                const base::string16& new_version) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::string16 message;
  Status status = FAILED;

  switch (error_code) {
    case GOOGLE_UPDATE_DISABLED_BY_POLICY:
      status = DISABLED_BY_ADMIN;
      message =
          l10n_util::GetStringUTF16(IDS_UPGRADE_DISABLED_BY_POLICY);
      break;
    case GOOGLE_UPDATE_DISABLED_BY_POLICY_AUTO_ONLY:
      status = DISABLED_BY_ADMIN;
      message =
          l10n_util::GetStringUTF16(IDS_UPGRADE_DISABLED_BY_POLICY_MANUAL);
      break;
    default:
      // html_error_message mentions error_code so don't combine messages.
      if (html_error_message.empty()) {
        message = l10n_util::GetStringFUTF16Int(IDS_UPGRADE_ERROR, error_code);
      } else {
        message = l10n_util::GetStringFUTF16(
            IDS_ABOUT_BOX_ERROR_DURING_UPDATE_CHECK, html_error_message);
      }
      break;
  }
  callback_.Run(status, 0, false, std::string(), 0, message);
}

void VersionUpdaterWin::DoBeginUpdateCheck(bool install_update_if_possible) {
  // Disconnect from any previous attempts to avoid redundant callbacks.
  weak_factory_.InvalidateWeakPtrs();
  BeginUpdateCheck(g_browser_process->GetApplicationLocale(),
                   install_update_if_possible, owner_widget_,
                   weak_factory_.GetWeakPtr());
}

void VersionUpdaterWin::OnPendingRestartCheck(bool is_update_pending_restart) {
  callback_.Run(is_update_pending_restart ? NEARLY_UPDATED : UPDATED, 0, false,
                std::string(), 0, base::string16());
}

VersionUpdater* VersionUpdater::Create(content::WebContents* web_contents) {
  // Retrieve the HWND for the browser window that is hosting the update check.
  // This will be used as the parent for a UAC prompt, if needed. It's possible
  // this this window will no longer have focus by the time UAC is needed. In
  // that case, the UAC prompt will appear in the taskbar and will require a
  // user click. This is the least surprising thing we can do for the user, and
  // is the intended behavior for Windows applications.
  // It's also possible that the browser window hosting the update check will
  // have been closed by the time the UAC prompt is needed. In this case, the
  // web contents may no longer be hosted in a window, leading either
  // GetTopLevelNativeWindow or GetHost to return null. Passing nullptr to
  // VersionUpdaterWin will then also cause the UAC prompt to appear in the task
  // bar.
  gfx::NativeWindow window = web_contents->GetTopLevelNativeWindow();
  aura::WindowTreeHost* window_tree_host = window ? window->GetHost() : nullptr;
  return new VersionUpdaterWin(
      window_tree_host ? window_tree_host->GetAcceleratedWidget() : nullptr);
}
