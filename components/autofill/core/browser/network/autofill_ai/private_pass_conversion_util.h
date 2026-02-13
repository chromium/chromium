// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PRIVATE_PASS_CONVERSION_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PRIVATE_PASS_CONVERSION_UTIL_H_

namespace wallet {
class PrivatePass;
}

namespace autofill {

class EntityInstance;

// Converts an `EntityInstance` of a private pass type to a
// `wallet::PrivatePass`, used for communication with the Wallet API.
wallet::PrivatePass EntityInstanceToPrivatePass(const EntityInstance& entity);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_NETWORK_AUTOFILL_AI_PRIVATE_PASS_CONVERSION_UTIL_H_
