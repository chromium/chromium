// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_DIGITAL_IDENTITY_PROVIDER_DESKTOP_H_
#define CHROME_BROWSER_WEBID_DIGITAL_IDENTITY_PROVIDER_DESKTOP_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/digital_identity_provider.h"

namespace content {
class WebContents;
}

// Desktop-specific implementation of `DigitalIdentityProvider`. Uses FIDO
// hybrid flow to retrieve credentials stored on a mobile device.
class DigitalIdentityProviderDesktop : public content::DigitalIdentityProvider {
 public:
  DigitalIdentityProviderDesktop();
  ~DigitalIdentityProviderDesktop() override;

  void Request(content::WebContents* web_contents,
               const url::Origin& rp_origin,
               const std::string& request,
               DigitalIdentityCallback callback) override;

 private:
  // Shows dialog with QR code.
  void ShowQrCodeDialog(content::WebContents* web_contents,
                        const url::Origin& rp_origin,
                        const std::string& qr_url);

  // Called when the QR code dialog is closed.
  void OnQrCodeDialogCanceled();

  DigitalIdentityCallback callback_;

  base::WeakPtrFactory<DigitalIdentityProviderDesktop> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBID_DIGITAL_IDENTITY_PROVIDER_DESKTOP_H_
