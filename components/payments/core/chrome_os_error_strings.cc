// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/chrome_os_error_strings.h"

namespace payments {
namespace errors {

const char kEmptyResponse[] = "Android app response is empty.";

const char kInvalidResponse[] = "Android app response is not valid.";

const char kMoreThanOneActivity[] =
    "Found more than one PAY activity in the Trusted Web Activity, but at most "
    "one activity is supported.";

const char kMoreThanOneMethodData[] =
    "At most one payment method specific data is supported.";

const char kUnableToConnectToAsh[] =
    "Cannot connect to the OS from Lacros browser.";

}  // namespace errors
}  // namespace payments
