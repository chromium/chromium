// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ssl_client_certificate_selector.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ssl/ssl_client_auth_observer.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace {

// Returns the storage of a test hook for `ShowSSLClientCertificateSelector()`.
ShowSSLClientCertificateSelectorTestingHook&
GetShowSSLClientCertificateSelectorTestingHook() {
  static base::NoDestructor<ShowSSLClientCertificateSelectorTestingHook>
      instance;
  return *instance;
}

}  // namespace

class SSLClientCertificateSelector::SSLClientAuthObserverImpl
    : public SSLClientAuthObserver,
      public content::WebContentsObserver {
 public:
  SSLClientAuthObserverImpl(
      content::WebContents* web_contents,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
      std::unique_ptr<content::ClientCertificateDelegate> delegate)
      : SSLClientAuthObserver(web_contents->GetBrowserContext(),
                              cert_request_info,
                              std::move(delegate)),
        content::WebContentsObserver(web_contents) {}

  void Init(base::OnceClosure close_dialog_callback) {
    close_dialog_callback_ = std::move(close_dialog_callback);
    StartObserving();
  }

  static void AcceptCertificate(
      std::unique_ptr<SSLClientAuthObserverImpl> self,
      std::unique_ptr<net::ClientCertIdentity> identity) {
    // Remove the observer before we try acquiring private key, otherwise we
    // might act on a notification while waiting for the callback, causing us
    // to delete ourself before the callback gets called, or to try to run
    // |close_dialog_callback_| on a dialog which is already closed.
    self->StopObserving();
    net::X509Certificate* cert = identity->certificate();
    net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
        std::move(identity),
        base::BindOnce(&SSLClientAuthObserverImpl::GotPrivateKey,
                       std::move(self), base::Unretained(cert)));
  }

  void GotPrivateKey(net::X509Certificate* cert,
                     scoped_refptr<net::SSLPrivateKey> private_key) {
    CertificateSelected(cert, private_key.get());
  }

  // SSLClientAuthObserver:
  void OnCertSelectedByNotification() override {
    std::move(close_dialog_callback_).Run();
  }

  // content::WebContentsObserver:
  void WebContentsDestroyed() override {
    // If the tab is closed (either while the selector dialog is still showing,
    // or after the dialog has closed but the AcquirePrivateKey callback is
    // still pending), abort the request.
    CancelCertificateSelection();
  }

 private:
  base::OnceClosure close_dialog_callback_;
};

SSLClientCertificateSelector::SSLClientCertificateSelector(
    content::WebContents* web_contents,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate)
    : CertificateSelector(std::move(client_certs), web_contents),
      auth_observer_impl_(
          std::make_unique<SSLClientAuthObserverImpl>(web_contents,
                                                      cert_request_info,
                                                      std::move(delegate))) {
  RegisterDeleteDelegateCallback(base::BindOnce(
      [](SSLClientCertificateSelector* dialog) {
        // This is here and not in Cancel() to give WebContentsDestroyed a
        // chance to abort instead of proceeding with a null certificate. (This
        // will be ignored if there was a previous call to CertificateSelected
        // or CancelCertificateSelection.)
        if (dialog->auth_observer_impl_)
          dialog->auth_observer_impl_->CertificateSelected(nullptr, nullptr);
      },
      this));
}

SSLClientCertificateSelector::~SSLClientCertificateSelector() {}

void SSLClientCertificateSelector::Init() {
  auth_observer_impl_->Init(base::BindOnce(
      &SSLClientCertificateSelector::CloseDialog, base::Unretained(this)));
  std::unique_ptr<views::Label> text_label(
      new views::Label(l10n_util::GetStringFUTF16(
          IDS_CLIENT_CERT_DIALOG_TEXT,
          base::ASCIIToUTF16(auth_observer_impl_->cert_request_info()
                                 ->host_and_port.ToString()))));
  text_label->SetMultiLine(true);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label->SetAllowCharacterBreak(true);
  text_label->SizeToFit(kTableViewWidth);
  InitWithText(std::move(text_label));
}

void SSLClientCertificateSelector::CloseDialog() {
  GetWidget()->Close();
}

void SSLClientCertificateSelector::AcceptCertificate(
    std::unique_ptr<net::ClientCertIdentity> identity) {
  // The SSLClientCertificateSelector will be destroyed after this method
  // returns, so the SSLClientAuthObserverImpl manages its own lifetime while
  // acquiring the private key from |identity|.
  SSLClientAuthObserverImpl::AcceptCertificate(std::move(auth_observer_impl_),
                                               std::move(identity));
}

void SSLClientCertificateSelector::OnCancel() {
  // Close the dialog if it is not currently being displayed
  if (!GetWidget()->IsVisible())
    CloseDialog();
}

base::OnceClosure SSLClientCertificateSelector::GetCancellationCallback() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::BindOnce(&SSLClientCertificateSelector::OnCancel,
                        weak_factory_.GetWeakPtr());
}

base::OnceClosure ShowSSLClientCertificateSelector(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!GetShowSSLClientCertificateSelectorTestingHook().is_null()) {
    return GetShowSSLClientCertificateSelectorTestingHook().Run(
        contents, cert_request_info, std::move(client_certs),
        std::move(delegate));
  }

  // Don't bother prompting the user if there are no certs to choose from.
  // Just continue with no certificate.
  if (client_certs.empty()) {
    delegate->ContinueWithCertificate(nullptr, nullptr);
    return base::OnceClosure();
  }

  // Not all WebContentses can show modal dialogs.
  //
  // TODO(davidben): Move this hook to the WebContentsDelegate and only try to
  // show a dialog in Browser's implementation. https://crbug.com/456255
  if (!SSLClientCertificateSelector::CanShow(contents))
    return base::OnceClosure();

  SSLClientCertificateSelector* selector = new SSLClientCertificateSelector(
      contents, cert_request_info, std::move(client_certs),
      std::move(delegate));
  selector->Init();
  selector->Show();
  return selector->GetCancellationCallback();
}

void SetShowSSLClientCertificateSelectorHookForTest(
    ShowSSLClientCertificateSelectorTestingHook hook) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetShowSSLClientCertificateSelectorTestingHook() = std::move(hook);
}
