// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cookie_config/cookie_store_util.h"

#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"

namespace cookie_config {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
namespace {

// Use the operating system's mechanisms to encrypt cookies before writing
// them to persistent store.  Currently this only is done with desktop OS's
// because ChromeOS and Android already protect the entire profile contents.
class CookieOSCryptoDelegate : public net::CookieCryptoDelegate {
 public:
  void Init(base::OnceClosure callback) override;
  bool EncryptString(const std::string& plaintext,
                     std::string* ciphertext) override;
  bool DecryptString(const std::string& ciphertext,
                     std::string* plaintext) override;
};

void CookieOSCryptoDelegate::Init(base::OnceClosure callback) {
  std::move(callback).Run();
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
#else   // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
net::CookieCryptoDelegate* GetCookieCryptoDelegate() {
  return nullptr;
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace cookie_config
