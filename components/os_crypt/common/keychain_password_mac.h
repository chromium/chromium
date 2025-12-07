// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_COMMON_KEYCHAIN_PASSWORD_MAC_H_
#define COMPONENTS_OS_CRYPT_COMMON_KEYCHAIN_PASSWORD_MAC_H_

#include <string>

#include "base/memory/raw_ref.h"

namespace crypto::apple {
class Keychain;
}

class KeychainPassword {
 public:
#if defined(ALLOW_RUNTIME_CONFIGURABLE_KEY_STORAGE)
  using KeychainNameType = std::string;
#else
  using KeychainNameType = const std::string;
#endif

  explicit KeychainPassword(const crypto::apple::Keychain& keychain);

  KeychainPassword(const KeychainPassword&) = delete;
  KeychainPassword& operator=(const KeychainPassword&) = delete;

  ~KeychainPassword();

  // Get the OSCrypt password for this system. If no password exists
  // in the Keychain then one is generated, stored in the Mac keychain, and
  // returned.
  // If one exists then it is fetched from the Keychain and returned.
  // If the user disallows access to the Keychain (or an error occurs) then an
  // empty string is returned.
  std::string GetPassword() const;

  // The service and account names used in Chrome's Safe Storage keychain item.
  static KeychainNameType& GetServiceName();
  static KeychainNameType& GetAccountName();

 private:
  const raw_ref<const crypto::apple::Keychain> keychain_;
};

#endif  // COMPONENTS_OS_CRYPT_COMMON_KEYCHAIN_PASSWORD_MAC_H_
