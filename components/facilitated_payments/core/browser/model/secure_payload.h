// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MODEL_SECURE_PAYLOAD_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MODEL_SECURE_PAYLOAD_H_

#include <cstdint>
#include <string>
#include <vector>

namespace payments::facilitated {

// Contains the decrypted data received from Payments backend.
struct SecureData {
  int32_t key;
  std::string value;
};

// Contains the action token and the decrypted data from Payments
// backend. Both are required to trigger a UI flow within Google Play Services.
struct SecurePayload {
  std::vector<uint8_t> action_token;
  std::vector<SecureData> secure_data;

  SecurePayload();
  SecurePayload(const SecurePayload& other);
  SecurePayload& operator=(const SecurePayload& other);
  ~SecurePayload();
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_BROWSER_MODEL_SECURE_PAYLOAD_H_
