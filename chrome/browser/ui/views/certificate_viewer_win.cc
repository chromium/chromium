// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_viewer.h"

#include <windows.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/threading/thread.h"
#include "chrome/browser/ui/cryptuiapi_shim.h"
#include "crypto/scoped_capi_types.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_win.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/shell_dialogs/base_shell_dialog_win.h"

namespace {

// Shows a Windows certificate viewer dialog on a background thread to avoid
// nested run loops.
class CertificateViewerDialogWin : public ui::BaseShellDialogImpl {
 public:
  CertificateViewerDialogWin() {}

  CertificateViewerDialogWin(const CertificateViewerDialogWin&) = delete;
  CertificateViewerDialogWin& operator=(const CertificateViewerDialogWin&) =
      delete;

  // Shows the dialog and calls |callback| when the dialog closes. The caller
  // must ensure the CertificateViewerDialogWin remains valid until then.
  void Show(HWND parent,
            net::X509Certificate* cert,
            base::RepeatingClosure callback) {
    if (IsRunningDialogForOwner(parent)) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                                  callback);
      return;
    }

    std::unique_ptr<RunState> run_state = BeginRun(parent);

    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        run_state->dialog_task_runner;
    task_runner->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&CertificateViewerDialogWin::ShowOnDialogThread,
                       base::Unretained(this), parent,
                       base::WrapRefCounted(cert)),
        base::BindOnce(&CertificateViewerDialogWin::OnDialogClosed,
                       base::Unretained(this), std::move(run_state), callback));
  }

  static bool mock_certificate_viewer_for_testing;

 private:
  void ShowOnDialogThread(HWND owner,
                          const scoped_refptr<net::X509Certificate>& cert) {
    // Create a new cert context and store containing just the certificate
    // and its intermediate certificates.
    crypto::ScopedPCCERT_CONTEXT cert_list(
        net::x509_util::CreateCertContextWithChain(cert.get()));
    // Perhaps this should show an error instead of silently failing, but it's
    // probably not even possible to get here with a cert that can't be
    // converted to a CERT_CONTEXT.
    if (!cert_list)
      return;

    CRYPTUI_VIEWCERTIFICATE_STRUCT view_info = {0};
    view_info.dwSize = sizeof(view_info);
    view_info.hwndParent = owner;
    view_info.dwFlags =
        CRYPTUI_DISABLE_EDITPROPERTIES | CRYPTUI_DISABLE_ADDTOSTORE;
    view_info.pCertContext = cert_list.get();
    HCERTSTORE cert_store = cert_list->hCertStore;
    view_info.cStores = 1;
    view_info.rghStores = &cert_store;

    if (mock_certificate_viewer_for_testing)
      return;

    BOOL properties_changed;
    ::CryptUIDlgViewCertificate(&view_info, &properties_changed);
  }

  void OnDialogClosed(std::unique_ptr<RunState> run_state,
                      base::OnceClosure callback) {
    EndRun(std::move(run_state));
    // May delete |this|.
    std::move(callback).Run();
  }
};

// static
bool CertificateViewerDialogWin::mock_certificate_viewer_for_testing = false;

}  // namespace

void ShowCertificateViewerForClientAuth(content::WebContents* web_contents,
                                        gfx::NativeWindow parent,
                                        net::X509Certificate* cert) {
  CertificateViewerDialogWin* dialog = new CertificateViewerDialogWin;
  dialog->Show(parent->GetHost()->GetAcceleratedWidget(), cert,
               base::BindRepeating(
                   &base::DeletePointer<CertificateViewerDialogWin>, dialog));
}

void MockCertificateViewerForTesting() {
  CertificateViewerDialogWin::mock_certificate_viewer_for_testing = true;
}
