// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_UTIL_H_

#include "components/webdata/common/web_database.h"

namespace autofill {

class AutofillWebDataBackendImpl;

namespace util {

// Converts server profiles to local profiles. The task only converts profiles
// that have not been converted before.
WebDatabase::State ConvertWalletAddressesAndUpdateWalletCards(
    const std::string& app_locale,
    const std::string& primary_account_email,
    AutofillWebDataBackendImpl* backend,
    WebDatabase* db);

}  // namespace util
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WEBDATA_BACKEND_UTIL_H_
