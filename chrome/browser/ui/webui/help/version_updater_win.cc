// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/thread_pool.h"
#include "base/win/win_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/browser/google/google_update_win.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"

namespace {

// Windows implementation of version update functionality, used by the WebUI
// About/Help page.
class VersionUpdaterWin : public VersionUpdater, public UpdateCheckDelegate {
 public:
  // |owner_widget| is the parent widget hosting the update check UI. Any UI
  // needed to install an update (e.g., a UAC prompt for a system-level install)
  // will be parented to this widget. |owner_widget| may be given a value of
  // nullptr in which case the UAC prompt will be parented to the desktop.
  explicit VersionUpdaterWin(gfx::AcceleratedWidget owner_widget)
      : owner_widget_(owner_widget), weak_factory_(this) {}

  VersionUpdaterWin(const VersionUpdaterWin&) = delete;
  VersionUpdaterWin& operator=(const VersionUpdaterWin&) = delete;

  ~VersionUpdaterWin() override = default;

  // VersionUpdater:
  void CheckForUpdate(StatusCallback callback, PromoteCallback) override {
    // There is no supported integration with Google Update for Chromium.
    callback_ = std::move(callback);

    callback_.Run(CHECKING, 0, false, false, std::string(), 0,
                  std::u16string());
    DoBeginUpdateCheck(false /* !install_update_if_possible */);
  }

  // UpdateCheckDelegate:
  void OnUpdateCheckComplete(const std::u16string& new_version) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (new_version.empty()) {
      // Google Update says that no new version is available. Check to see if a
      // restart is needed for a previously-applied update to take effect.
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&upgrade_util::IsUpdatePendingRestart),
          base::BindOnce(&VersionUpdaterWin::OnPendingRestartCheck,
                         weak_factory_.GetWeakPtr()));
      // Early exit since callback_ will be Run in OnPendingRestartCheck.
      return;
    }

    // Notify the caller that the update is now beginning and initiate it.
    DoBeginUpdateCheck(true /* install_update_if_possible */);
    callback_.Run(UPDATING, 0, false, false, std::string(), 0,
                  std::u16string());
  }

  void OnUpgradeProgress(int progress,
                         const std::u16string& new_version) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    callback_.Run(UPDATING, progress, false, false, std::string(), 0,
                  std::u16string());
  }

  void OnUpgradeComplete(const std::u16string& new_version) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    callback_.Run(NEARLY_UPDATED, 0, false, false, std::string(), 0,
                  std::u16string());
  }

  void OnError(GoogleUpdateErrorCode error_code,
               const std::u16string& html_error_message,
               const std::u16string& new_version) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    std::u16string message;
    Status status = FAILED;

    switch (error_code) {
      case GOOGLE_UPDATE_DISABLED_BY_POLICY:
        status = DISABLED_BY_ADMIN;
        message = l10n_util::GetStringUTF16(IDS_UPGRADE_DISABLED_BY_POLICY);
        break;
      case GOOGLE_UPDATE_DISABLED_BY_POLICY_AUTO_ONLY:
        status = DISABLED_BY_ADMIN;
        message =
            l10n_util::GetStringUTF16(IDS_UPGRADE_DISABLED_BY_POLICY_MANUAL);
        break;
      default:
        // html_error_message mentions error_code so don't combine messages.
        if (html_error_message.empty()) {
          message =
              l10n_util::GetStringFUTF16Int(IDS_UPGRADE_ERROR, error_code);
        } else {
          message = l10n_util::GetStringFUTF16(
              IDS_ABOUT_BOX_ERROR_DURING_UPDATE_CHECK, html_error_message);
        }
        break;
    }
    callback_.Run(status, 0, false, false, std::string(), 0, message);
  }

 private:
  void DoBeginUpdateCheck(bool install_update_if_possible) {
    // Disconnect from any previous attempts to avoid redundant callbacks.
    weak_factory_.InvalidateWeakPtrs();
    BeginUpdateCheck(g_browser_process->GetApplicationLocale(),
                     install_update_if_possible, owner_widget_,
                     weak_factory_.GetWeakPtr());
  }

  // A task run on the UI thread with the result of checking for a pending
  // restart.
  void OnPendingRestartCheck(bool is_update_pending_restart) {
    callback_.Run(is_update_pending_restart ? NEARLY_UPDATED : UPDATED, 0,
                  false, false, std::string(), 0, std::u16string());
  }

  // The widget owning the UI for the update check.
  gfx::AcceleratedWidget owner_widget_;

  // Callback used to communicate update status to the client.
  StatusCallback callback_;

  // Used for callbacks.
  base::WeakPtrFactory<VersionUpdaterWin> weak_factory_;
};

}  // namespace

std::unique_ptr<VersionUpdater> VersionUpdater::Create(
    content::WebContents* web_contents) {
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
  return std::make_unique<VersionUpdaterWin>(
      window_tree_host ? window_tree_host->GetAcceleratedWidget() : nullptr);
}
