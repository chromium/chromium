// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_DEVICE_REAUTH_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_DEVICE_REAUTH_UTIL_H_

namespace autofill {

class AutofillClient;

// Returns `true` if the device supports any kind of re-auth.
bool SupportsDeviceReauth(const AutofillClient& client);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERMISSIONS_DEVICE_REAUTH_UTIL_H_
