// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PLATFORM_KEYS_CERTIFICATE_SELECTOR_CHROMEOS_H_
#define CHROME_BROWSER_UI_PLATFORM_KEYS_CERTIFICATE_SELECTOR_CHROMEOS_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"

namespace content {
class WebContents;
}

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;
}  // namespace net

namespace chromeos {

typedef base::Callback<void(
    const scoped_refptr<net::X509Certificate>& selected_certificate)>
    CertificateSelectedCallback;

// Opens a constrained client certificate selection dialog associated with
// |web_contents|, offering |certificates| to the user and explaining that the
// selection will grant access to the extension with |extension_id|.
// When the user has made a selection, the dialog will report back to
// |callback|. |callback| is notified when the dialog closes in call cases; if
// the user cancels the dialog, |callback| will be called with a nullptr
// argument.
void ShowPlatformKeysCertificateSelector(
    content::WebContents* web_contents,
    const std::string& extension_id,
    const net::CertificateList& certificates,
    const CertificateSelectedCallback& callback);

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_PLATFORM_KEYS_CERTIFICATE_SELECTOR_CHROMEOS_H_
