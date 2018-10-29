// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Generates fingerprints appropriate for sending to the Google Wallet Risk
// engine, which is the fraud-detection engine used for purchases powered by
// Google Wallet.  A fingerprint encapsulates machine and user characteristics.
// Because much of the data is privacy-sensitive, fingerprints should only be
// generated with explicit user consent, including consent to gather geolocation
// data.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_RISK_FINGERPRINT_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_RISK_FINGERPRINT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "components/autofill/core/browser/autofill_client.h"

namespace base {
class Time;
}

namespace content {
class WebContents;
}

namespace service_manager {
class Connector;
}

namespace gfx {
class Rect;
}

namespace autofill {
namespace risk {

class Fingerprint;

// Asynchronously calls |callback| with statistics that, collectively, provide a
// unique fingerprint for this (machine, user) pair, used for fraud prevention.
// |obfuscated_gaia_id| is an obfuscated user id for Google's authentication
// system. |window_bounds| should be the bounds of the containing Chrome window.
// |web_contents| should be the host for the page the user is interacting with.
// |version| is the version number of the application. |charset| is the default
// character set. |accept_languages| is the Accept-Languages setting.
// |install_time| is the absolute time of installation.
void GetFingerprint(
    uint64_t obfuscated_gaia_id,
    const gfx::Rect& window_bounds,
    content::WebContents* web_contents,
    const std::string& version,
    const std::string& charset,
    const std::string& accept_languages,
    const base::Time& install_time,
    const std::string& app_locale,
    const std::string& user_agent,
    const base::OnceCallback<void(std::unique_ptr<Fingerprint>)> callback,
    service_manager::Connector* connector);

}  // namespace risk
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_RISK_FINGERPRINT_H_
