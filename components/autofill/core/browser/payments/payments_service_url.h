// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_SERVICE_URL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_SERVICE_URL_H_

#include <stddef.h>
#include <stdint.h>

class GURL;

namespace url {
class Origin;
}

namespace autofill::payments {

// Returns true if production Payments URLs should be used or false if sandbox
// should be used.
bool IsPaymentsProductionEnabled();

// Returns the base URL to use for calls to Google Payments endpoints.
GURL GetBaseSecureUrl();

// Returns the Origin used by Google Pay's pay.js script
url::Origin GetGooglePayScriptOrigin();

// Returns the URL to navigate to in order to allow the user to edit or delete
// payment instruments (credit cards) or addresses, respectively.
// `GetManageInstrumentsUrl` redirects to the top level page that contains a
// list of instruments while `GetManageInstrumentUrl` redirects to the detail
// page for a particular instrument given the `instrument_id`.
GURL GetManageInstrumentsUrl();
GURL GetManageInstrumentUrl(int64_t instrument_id);
GURL GetManageAddressesUrl();

// Returns the support URL for users to learn more about virtual cards during
// the virtual card enrollment bubble.
GURL GetVirtualCardEnrollmentSupportUrl();

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_SERVICE_URL_H_
