// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cookie_config/cookie_store_util.h"

#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"

namespace cookie_config {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
namespace {

// Use the operating system's mechanisms to encrypt cookies before writing
// them to persistent store.  Currently this only is done with desktop OS's
// because ChromeOS and Android already protect the entire profile contents.
class CookieOSCryptoDelegate : public net::CookieCryptoDelegate {
 public:
  bool ShouldEncrypt() override;
  bool EncryptString(const std::string& plaintext,
                     std::string* ciphertext) override;
  bool DecryptString(const std::string& ciphertext,
                     std::string* plaintext) override;
};

bool CookieOSCryptoDelegate::ShouldEncrypt() {
#if BUILDFLAG(IS_IOS)
  // Cookie encryption is not necessary on iOS, due to OS-protected storage.
  // However, due to https://codereview.chromium.org/135183021/, cookies were
  // accidentally encrypted. In order to allow these cookies to still be used,a
  // a CookieCryptoDelegate is provided that can decrypt existing cookies.
  // However, new cookies will not be encrypted. The alternatives considered
  // were not supplying a delegate at all (thus invalidating all existing
  // encrypted cookies) or in migrating all cookies at once, which may impose
  // startup costs.  Eventually, all cookies will get migrated as they are
  // rewritten.
  return false;
#else
  return true;
#endif
}

bool CookieOSCryptoDelegate::EncryptString(const std::string& plaintext,
                                           std::string* ciphertext) {
  return OSCrypt::EncryptString(plaintext, ciphertext);
}

bool CookieOSCryptoDelegate::DecryptString(const std::string& ciphertext,
                                           std::string* plaintext) {
  return OSCrypt::DecryptString(ciphertext, plaintext);
}

// Using a LazyInstance is safe here because this class is stateless and
// requires 0 initialization.
base::LazyInstance<CookieOSCryptoDelegate>::DestructorAtExit
    g_cookie_crypto_delegate = LAZY_INSTANCE_INITIALIZER;

}  // namespace

net::CookieCryptoDelegate* GetCookieCryptoDelegate() {
  return g_cookie_crypto_delegate.Pointer();
}
#else   // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
net::CookieCryptoDelegate* GetCookieCryptoDelegate() {
  return NULL;
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace cookie_config
