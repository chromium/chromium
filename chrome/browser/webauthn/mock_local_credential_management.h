// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_MOCK_LOCAL_CREDENTIAL_MANAGEMENT_H_
#define CHROME_BROWSER_WEBAUTHN_MOCK_LOCAL_CREDENTIAL_MANAGEMENT_H_

#include "chrome/browser/webauthn/local_credential_management.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockLocalCredentialManagement : public LocalCredentialManagement {
 public:
  MockLocalCredentialManagement();

  ~MockLocalCredentialManagement() override;

  MOCK_METHOD(void,
              HasCredentials,
              (base::OnceCallback<void(bool)>),
              (override));

  MOCK_METHOD(
      void,
      Enumerate,
      (base::OnceCallback<void(std::optional<std::vector<
                                   device::DiscoverableCredentialMetadata>>)>),
      (override));

  MOCK_METHOD(void,
              Delete,
              (base::span<const uint8_t>, base::OnceCallback<void(bool)>),
              (override));

  MOCK_METHOD(void,
              Edit,
              (base::span<uint8_t>,
               std::string,
               base::OnceCallback<void(bool)>),
              (override));
};

#endif  // CHROME_BROWSER_WEBAUTHN_MOCK_LOCAL_CREDENTIAL_MANAGEMENT_H_
