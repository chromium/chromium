// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"

#include <openssl/base.h>

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/certificate_viewer/certificate_viewer_webui.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"

void ShowCertificateDialog(base::WeakPtr<content::WebContents> web_contents,
                           bssl::UniquePtr<CRYPTO_BUFFER> cert) {
  if (!web_contents) {
    return;
  }

  CertificateViewerDialog::ShowConstrained(
      std::move(cert), web_contents.get(),
      web_contents->GetTopLevelNativeWindow());
}

void ShowCertificateDialog(
    base::WeakPtr<content::WebContents> web_contents,
    bssl::UniquePtr<CRYPTO_BUFFER> cert,
    chrome_browser_server_certificate_database::CertificateMetadata
        cert_metadata) {
  if (!web_contents) {
    return;
  }

  CertificateViewerDialog::ShowConstrainedWithMetadata(
      std::move(cert), std::move(cert_metadata), web_contents.get(),
      web_contents->GetTopLevelNativeWindow());
}

bool IsCACertificateManagementAllowed(const PrefService& prefs) {
  return prefs.GetInteger(prefs::kCACertificateManagementAllowed) !=
         static_cast<int>(CACertificateManagementPermission::kNone);
}
