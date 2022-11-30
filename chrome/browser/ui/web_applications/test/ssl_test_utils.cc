// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/ssl_test_utils.h"

#include "base/atomic_sequence_num.h"
#include "base/time/time.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/security_state/core/security_state.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_info.h"

namespace {

// NSS requires that serial numbers be unique even for the same issuer;
// as all fake certificates will contain the same issuer name, it's
// necessary to ensure the serial number is unique, as otherwise
// NSS will fail to parse.
base::AtomicSequenceNumber g_serial_number;

scoped_refptr<net::X509Certificate> CreateFakeCert() {
  std::unique_ptr<crypto::RSAPrivateKey> unused_key;
  std::string cert_der;
  if (!net::x509_util::CreateKeyAndSelfSignedCert(
          "CN=Error", static_cast<uint32_t>(g_serial_number.GetNext()),
          base::Time::Now() - base::Minutes(5),
          base::Time::Now() + base::Minutes(5), &unused_key, &cert_der)) {
    return nullptr;
  }
  return net::X509Certificate::CreateFromBytes(
      base::as_bytes(base::make_span(cert_der)));
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
