// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/ssl_test_utils.h"

#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/security_state/core/security_state.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_info.h"

namespace {

scoped_refptr<net::X509Certificate> CreateFakeCert() {
  std::vector<uint8_t> cert_der =
      net::x509_util::CreateUnusableCert("CN=Error");
  return net::X509Certificate::CreateFromBytes(cert_der);
}

}  // namespace

namespace web_app {

void CheckMixedContentLoaded(Browser* browser) {
  DCHECK(browser);
  ssl_test_util::CheckSecurityState(
      browser->tab_strip_model()->GetActiveWebContents(),
      ssl_test_util::CertError::NONE, security_state::WARNING,
      ssl_test_util::AuthState::DISPLAYED_INSECURE_CONTENT);
}

void CheckMixedContentFailedToLoad(Browser* browser) {
  DCHECK(browser);
  ssl_test_util::CheckSecurityState(
      browser->tab_strip_model()->GetActiveWebContents(),
      ssl_test_util::CertError::NONE, security_state::SECURE,
      ssl_test_util::AuthState::NONE);
}

void CreateFakeSslInfoCertificate(net::SSLInfo* ssl_info) {
  ssl_info->cert = ssl_info->unverified_cert = CreateFakeCert();
}

}  // namespace web_app
