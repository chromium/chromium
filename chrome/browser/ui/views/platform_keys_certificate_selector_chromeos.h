// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PLATFORM_KEYS_CERTIFICATE_SELECTOR_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_PLATFORM_KEYS_CERTIFICATE_SELECTOR_CHROMEOS_H_

#include <string>

#include "chrome/browser/ui/platform_keys_certificate_selector_chromeos.h"
#include "chrome/browser/ui/views/certificate_selector.h"
#include "net/cert/x509_certificate.h"

// Chrome should access this certificate selector only through the interface
// chrome/browser/ui/platform_keys_certificate_selector_chromeos.h .

namespace content {
class WebContents;
}

namespace chromeos {

// A certificate selector dialog that explains to the user that an extension
// requests access to certificates.
class PlatformKeysCertificateSelector : public chrome::CertificateSelector {
 public:
  // |callback| must not be null.
  PlatformKeysCertificateSelector(const net::CertificateList& certificates,
                                  const std::string& extension_name,
                                  CertificateSelectedCallback callback,
                                  content::WebContents* web_contents);

  PlatformKeysCertificateSelector(const PlatformKeysCertificateSelector&) =
      delete;
  PlatformKeysCertificateSelector& operator=(
      const PlatformKeysCertificateSelector&) = delete;

  ~PlatformKeysCertificateSelector() override;

  void Init();

  // chrome::CertificateSelector:
  void AcceptCertificate(
      std::unique_ptr<net::ClientCertIdentity> identity) override;

 private:
  const std::string extension_name_;

  // Will be reset to null after it was run.
  CertificateSelectedCallback callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_VIEWS_PLATFORM_KEYS_CERTIFICATE_SELECTOR_CHROMEOS_H_
