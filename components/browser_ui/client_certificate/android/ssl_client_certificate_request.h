// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_CLIENT_CERTIFICATE_ANDROID_SSL_CLIENT_CERTIFICATE_REQUEST_H_
#define COMPONENTS_BROWSER_UI_CLIENT_CERTIFICATE_ANDROID_SSL_CLIENT_CERTIFICATE_REQUEST_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace content {
class ClientCertificateDelegate;
class WebContents;
}  // namespace content

namespace net {
class SSLCertRequestInfo;
class SSLPrivateKey;
class X509Certificate;
}  // namespace net

namespace browser_ui {

// Opens a SSL client certificate selection dialog. Returns a callback that will
// cancel the dialog.
base::OnceClosure ShowSSLClientCertificateSelector(
    content::WebContents* contents,
    net::SSLCertRequestInfo* cert_request_info,
    std::unique_ptr<content::ClientCertificateDelegate> delegate);

size_t GetCountOfSSLClientCertificateSelectorForTesting(
    content::WebContents* contents);

// Simulates the system certificate selector completing the active request for
// `contents` with `cert` and `key` (both null if the user declined).
void CompleteActiveSSLClientCertificateRequestForTesting(
    content::WebContents* contents,
    scoped_refptr<net::X509Certificate> cert,
    scoped_refptr<net::SSLPrivateKey> key);

}  // namespace browser_ui

#endif  // COMPONENTS_BROWSER_UI_CLIENT_CERTIFICATE_ANDROID_SSL_CLIENT_CERTIFICATE_REQUEST_H_
