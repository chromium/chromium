// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_FAKE_MAGIC_ARCH_H_
#define CHROME_BROWSER_WEBAUTHN_FAKE_MAGIC_ARCH_H_

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

class FakeSecurityDomainService;
class FakeRecoveryKeyStore;

// Magic Arch is the name given to the service on accounts.google.com that
// performs recovery of Zuul and passkey security domain secrets. This fake
// implements some of the same operations so that tests can check that
// generated vaults are valid and contain the expected secret.
class FakeMagicArch {
 public:
  // Returns the security domain secret given a Google Password Manager PIN.
  static std::optional<std::vector<uint8_t>> RecoverWithPIN(
      std::string_view pin,
      const FakeSecurityDomainService& security_domain_service,
      const FakeRecoveryKeyStore& recovery_key_store);
};

#endif  // CHROME_BROWSER_WEBAUTHN_FAKE_MAGIC_ARCH_H_
