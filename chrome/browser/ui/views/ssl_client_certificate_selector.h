// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SSL_CLIENT_CERTIFICATE_SELECTOR_H_
#define CHROME_BROWSER_UI_VIEWS_SSL_CLIENT_CERTIFICATE_SELECTOR_H_

#include "base/macros.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/ui/views/certificate_selector.h"

// This header file exists only for testing.  Chrome should access the
// certificate selector only through the cross-platform interface
// chrome/browser/ssl_client_certificate_selector.h.

namespace content {
class WebContents;
}

namespace net {
class SSLCertRequestInfo;
}

class SSLClientCertificateSelector : public chrome::CertificateSelector {
 public:
  // Writes a callback to the output parameter |cancellation_callback|. The
  // callback expects to be invoked on the UI thread and will invoke this
  // instance's OnCancel.
  SSLClientCertificateSelector(
      content::WebContents* web_contents,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate);
  ~SSLClientCertificateSelector() override;

  void Init();
  void CloseDialog();

  // chrome::CertificateSelector:
  void DeleteDelegate() override;
  void AcceptCertificate(
      std::unique_ptr<net::ClientCertIdentity> identity) override;

  void OnCancel();

  // Returns a UI-thread callback that will cancel this CertificateSelector. The
  // callback may be null. The callback is not required to be called.
  base::OnceClosure GetCancellationCallback();

 private:
  class SSLClientAuthObserverImpl;

  std::unique_ptr<SSLClientAuthObserverImpl> auth_observer_impl_;

  base::WeakPtrFactory<SSLClientCertificateSelector> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SSLClientCertificateSelector);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SSL_CLIENT_CERTIFICATE_SELECTOR_H_
