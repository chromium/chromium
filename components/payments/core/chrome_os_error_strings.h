// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_CHROME_OS_ERROR_STRINGS_H_
#define COMPONENTS_PAYMENTS_CORE_CHROME_OS_ERROR_STRINGS_H_

namespace payments {
namespace errors {

// Developer facing error messages that are used only on Chrome OS.

// Used if ARC sends a null object to the browser.
extern const char kEmptyResponse[];

// Used if ARC sends an object ot the browser that has neither an error message
// nor a valid response.
extern const char kInvalidResponse[];

// Used when the TWA declares more than one PAY activity.
extern const char kMoreThanOneActivity[];

// Used when the merchant invokes the Trusted Web Activity with more than set of
// payment method specific data.
extern const char kMoreThanOneMethodData[];

// Used if Lacros service is down and cannot connect to Ash.
extern const char kUnableToConnectToAsh[];

}  // namespace errors
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_CHROME_OS_ERROR_STRINGS_H_
