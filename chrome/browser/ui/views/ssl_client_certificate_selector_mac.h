// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SSL_CLIENT_CERTIFICATE_SELECTOR_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_SSL_CLIENT_CERTIFICATE_SELECTOR_MAC_H_

#include <memory>

#include "base/callback.h"
#include "net/ssl/client_cert_identity.h"

// This header file exists only for testing.  Chrome should access the
// certificate selector only through the cross-platform interface
// chrome/browser/ssl_client_certificate_selector.h.

namespace content {
class ClientCertificateDelegate;
class WebContents;
}  // namespace content

namespace net {
class SSLCertRequestInfo;
}

namespace chrome {

class OkAndCancelableForTesting {
 public:
  virtual void ClickOkButton() = 0;
  virtual void ClickCancelButton() = 0;
};

OkAndCancelableForTesting* ShowSSLClientCertificateSelectorMacForTesting(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate,
    base::OnceClosure dealloc_closure);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_SSL_CLIENT_CERTIFICATE_SELECTOR_MAC_H_
