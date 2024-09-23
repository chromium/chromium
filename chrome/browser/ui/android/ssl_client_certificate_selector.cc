// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "components/browser_ui/client_certificate/android/ssl_client_certificate_request.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "net/ssl/ssl_private_key.h"

namespace {

// Returns the storage of a test hook for `ShowSSLClientCertificateSelector()`.
ShowSSLClientCertificateSelectorTestingHook&
GetShowSSLClientCertificateSelectorTestingHook() {
  static base::NoDestructor<ShowSSLClientCertificateSelectorTestingHook>
      instance;
  return *instance;
}

}  // namespace

base::OnceClosure ShowSSLClientCertificateSelector(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList unused_client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  if (!GetShowSSLClientCertificateSelectorTestingHook().is_null()) {
    return GetShowSSLClientCertificateSelectorTestingHook().Run(
        contents, cert_request_info, /*client_certs=*/{}, std::move(delegate));
  }

  return browser_ui::ShowSSLClientCertificateSelector(
      contents, cert_request_info, std::move(delegate));
}

void SetShowSSLClientCertificateSelectorHookForTest(
    ShowSSLClientCertificateSelectorTestingHook hook) {
  GetShowSSLClientCertificateSelectorTestingHook() = std::move(hook);
}

