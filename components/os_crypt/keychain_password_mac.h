// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_KEYCHAIN_PASSWORD_MAC_H_
#define COMPONENTS_OS_CRYPT_KEYCHAIN_PASSWORD_MAC_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/no_destructor.h"

namespace crypto {
class AppleKeychain;
}

class COMPONENT_EXPORT(OS_CRYPT) KeychainPassword {
 public:
#if defined(ALLOW_RUNTIME_CONFIGURABLE_KEY_STORAGE)
  using KeychainNameType = base::NoDestructor<std::string>;
#else
  using KeychainNameType = const base::NoDestructor<std::string>;
#endif

  KeychainPassword(const crypto::AppleKeychain& keychain);
  ~KeychainPassword();

  // Get the OSCrypt password for this system. If no password exists
  // in the Keychain then one is generated, stored in the Mac keychain, and
  // returned.
  // If one exists then it is fetched from the Keychain and returned.
  // If the user disallows access to the Keychain (or an error occurs) then an
  // empty string is returned.
  std::string GetPassword() const;

  // The service and account names used in Chrome's Safe Storage keychain item.
  static COMPONENT_EXPORT(OS_CRYPT) KeychainNameType service_name;
  static COMPONENT_EXPORT(OS_CRYPT) KeychainNameType account_name;

 private:
  const crypto::AppleKeychain& keychain_;

  DISALLOW_COPY_AND_ASSIGN(KeychainPassword);
};

#endif  // COMPONENTS_OS_CRYPT_KEYCHAIN_PASSWORD_MAC_H_
