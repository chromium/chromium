// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_utils.h"

#include <windows.h>

#include <shellapi.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cryptuiapi_shim.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/font.h"
#include "ui/shell_dialogs/base_shell_dialog_win.h"
#include "ui/views/win/hwnd_util.h"

namespace settings_utils {

namespace {

// Shows a Windows certificate management dialog on the dialog thread.
class ManageCertificatesDialog : public ui::BaseShellDialogImpl {
 public:
  ManageCertificatesDialog() = default;

  ManageCertificatesDialog(const ManageCertificatesDialog&) = delete;
  ManageCertificatesDialog& operator=(const ManageCertificatesDialog&) = delete;

  // Shows the dialog and calls |callback| when the dialog closes. The caller
  // must ensure the ManageCertificatesDialog remains valid until then.
  void Show(HWND parent, base::OnceClosure callback) {
    if (IsRunningDialogForOwner(parent)) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(callback));
      return;
    }

    std::unique_ptr<RunState> run_state = BeginRun(parent);

    base::SingleThreadTaskRunner* task_runner =
        run_state->dialog_task_runner.get();
    task_runner->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&ManageCertificatesDialog::ShowOnDialogThread,
                       base::Unretained(this), parent),
        base::BindOnce(&ManageCertificatesDialog::OnDialogClosed,
                       base::Unretained(this), std::move(run_state),
                       std::move(callback)));
  }

 private:
  void ShowOnDialogThread(HWND owner) {
    CRYPTUI_CERT_MGR_STRUCT cert_mgr = {0};
    cert_mgr.dwSize = sizeof(CRYPTUI_CERT_MGR_STRUCT);
    cert_mgr.hwndParent = owner;
    ::CryptUIDlgCertMgr(&cert_mgr);
  }

  void OnDialogClosed(std::unique_ptr<RunState> run_state,
                      base::OnceClosure callback) {
    EndRun(std::move(run_state));
    // May delete |this|.
    std::move(callback).Run();
  }
};

}  // namespace

// Callback that opens the Internet Options control panel dialog with the
// Connections tab selected.
void OpenConnectionDialogCallback() {
  // Using rundll32 seems better than LaunchConnectionDialog which causes a
  // new dialog to be made for each call.  rundll32 uses the same global
  // dialog and it seems to share with the shortcut in control panel.
  base::FilePath rundll32;
  base::PathService::Get(base::DIR_SYSTEM, &rundll32);
  rundll32 = rundll32.AppendASCII("rundll32.exe");

  base::FilePath shell32dll;
  base::PathService::Get(base::DIR_SYSTEM, &shell32dll);
  shell32dll = shell32dll.AppendASCII("shell32.dll");

  base::FilePath inetcpl;
  base::PathService::Get(base::DIR_SYSTEM, &inetcpl);
  inetcpl = inetcpl.AppendASCII("inetcpl.cpl,,4");

  std::wstring args(shell32dll.value());
  args.append(L",Control_RunDLL ");
  args.append(inetcpl.value());

  ShellExecute(NULL, L"open", rundll32.value().c_str(), args.c_str(), NULL,
               SW_SHOWNORMAL);
}

void ShowNetworkProxySettings(content::WebContents* /*web_contents*/) {
  // See
  // https://docs.microsoft.com/en-us/windows/uwp/launch-resume/launch-settings-app#network--internet
  platform_util::OpenExternal(GURL("ms-settings:network-proxy"));
}

void ShowManageSSLCertificates(content::WebContents* web_contents) {
  HWND parent =
      views::HWNDForNativeWindow(web_contents->GetTopLevelNativeWindow());

  ManageCertificatesDialog* dialog = new ManageCertificatesDialog;
  dialog->Show(
      parent,
      base::BindOnce(&base::DeletePointer<ManageCertificatesDialog>, dialog));
}

}  // namespace settings_utils
