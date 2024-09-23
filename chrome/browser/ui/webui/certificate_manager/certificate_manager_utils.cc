// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"

#include <openssl/base.h>

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/certificate_viewer_webui.h"
#include "content/public/browser/web_contents.h"

void ShowCertificateDialog(base::WeakPtr<content::WebContents> web_contents,
                           bssl::UniquePtr<CRYPTO_BUFFER> cert) {
  if (!web_contents) {
    return;
  }

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> view_certs;
  view_certs.push_back(std::move(cert));
  CertificateViewerDialog::ShowConstrained(
      std::move(view_certs),
      /*cert_nicknames=*/{}, web_contents.get(),
      web_contents->GetTopLevelNativeWindow());
}
