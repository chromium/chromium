// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_WEBUI_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_WEBUI_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "crypto/crypto_buildflags.h"
#include "net/cert/x509_certificate.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

#if BUILDFLAG(USE_NSS_CERTS)
#include "net/cert/scoped_nss_types.h"
#endif

namespace content {
class WebContents;
}

class ConstrainedWebDialogDelegate;

// Dialog for displaying detailed certificate information. This is used on
// desktop builds to display detailed information in a floating dialog when the
// user clicks on "Certificate Information" from the lock icon of a web site or
// "View" from the Certificate Manager.
class CertificateViewerDialog : public ui::WebDialogDelegate {
 public:
#if BUILDFLAG(USE_NSS_CERTS)
  static CertificateViewerDialog* ShowConstrained(
      net::ScopedCERTCertificateList nss_certs,
      content::WebContents* web_contents,
      gfx::NativeWindow parent);
#endif

  static CertificateViewerDialog* ShowConstrained(
      std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs,
      std::vector<std::string> cert_nicknames,
      content::WebContents* web_contents,
      gfx::NativeWindow parent);

  CertificateViewerDialog(const CertificateViewerDialog&) = delete;
  CertificateViewerDialog& operator=(const CertificateViewerDialog&) = delete;

  ~CertificateViewerDialog() override;

  gfx::NativeWindow GetNativeWebContentsModalDialog();

 private:
  friend class CertificateViewerUITest;

  // Construct a certificate viewer for the passed in certificate. A reference
  // to the certificate pointer is added for the lifetime of the certificate
  // viewer.
  CertificateViewerDialog(std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs,
                          std::vector<std::string> cert_nicknames);

  raw_ptr<ConstrainedWebDialogDelegate, DanglingUntriaged> delegate_ = nullptr;
};

// Dialog handler which handles calls from the JS WebUI code to view certificate
// details and export the certificate.
class CertificateViewerDialogHandler : public content::WebUIMessageHandler {
 public:
  CertificateViewerDialogHandler(
      CertificateViewerDialog* dialog,
      std::vector<x509_certificate_model::X509CertificateModel> certs);

  CertificateViewerDialogHandler(const CertificateViewerDialogHandler&) =
      delete;
  CertificateViewerDialogHandler& operator=(
      const CertificateViewerDialogHandler&) = delete;

  ~CertificateViewerDialogHandler() override;

  // Overridden from WebUIMessageHandler
  void RegisterMessages() override;

 private:
  // Brings up the export certificate dialog for the chosen certificate in the
  // chain.
  //
  // The input is an integer index to the certificate in the chain to export.
  void HandleExportCertificate(const base::Value::List& args);

  // Gets the details for a specific certificate in the certificate chain.
  // Responds with a tree structure containing the fields and values for certain
  // nodes.
  //
  // The input is an integer index to the certificate in the chain to view.
  void HandleRequestCertificateFields(const base::Value::List& args);

  // Helper function to get the certificate index. Returns -1 if the index is
  // out of range.
  int GetCertificateIndex(int requested_index) const;

  // The dialog.
  raw_ptr<CertificateViewerDialog> dialog_;

  std::vector<x509_certificate_model::X509CertificateModel> certs_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_WEBUI_H_
