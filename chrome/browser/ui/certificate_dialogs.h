// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CERTIFICATE_DIALOGS_H_
#define CHROME_BROWSER_UI_CERTIFICATE_DIALOGS_H_

#include "chrome/common/buildflags.h"
#include "crypto/crypto_buildflags.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if BUILDFLAG(USE_NSS_CERTS)
#include "net/cert/scoped_nss_types.h"
#endif

namespace content {
class WebContents;
}

void ShowCertSelectFileDialog(ui::SelectFileDialog* select_file_dialog,
                              ui::SelectFileDialog::Type type,
                              const base::FilePath& suggested_path,
                              gfx::NativeWindow parent);

// Show a dialog to save the first certificate or the whole chain.
void ShowCertExportDialog(content::WebContents* web_contents,
                          gfx::NativeWindow parent,
                          std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs,
                          const std::string& cert_title);

#if BUILDFLAG(USE_NSS_CERTS)
// Show a dialog to save the first certificate or the whole chain encompassed by
// the iterators.
void ShowCertExportDialog(content::WebContents* web_contents,
                          gfx::NativeWindow parent,
                          net::ScopedCERTCertificateList::iterator certs_begin,
                          net::ScopedCERTCertificateList::iterator certs_end);
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
// Show a dialog to save all the certs provided; certs are not necessarily part
// of any chain.
void ShowCertExportDialogSaveAll(
    content::WebContents* web_contents,
    gfx::NativeWindow parent,
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs,
    const std::string& suggested_file_name);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#endif  // CHROME_BROWSER_UI_CERTIFICATE_DIALOGS_H_
