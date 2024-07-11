// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_ENCRYPTION_HEADER_PARSERS_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_ENCRYPTION_HEADER_PARSERS_H_

#include <stdint.h>
#include <string>

#include "net/http/http_util.h"

namespace gcm {

// Iterates over a header that follows the syntax of the Encryption HTTP header
// per the Encrypted Content-Encoding for HTTP draft. This header follows the
// #list syntax from the extended ABNF syntax defined in section 1.2 of RFC7230.
//
// https://tools.ietf.org/html/draft-thomson-http-encryption#section-3
// https://tools.ietf.org/html/rfc7230#section-1.2
class EncryptionHeaderIterator {
 public:
  EncryptionHeaderIterator(std::string::const_iterator header_begin,
                           std::string::const_iterator header_end);
  ~EncryptionHeaderIterator();

  // Advances the iterator to the next header value, if any. Returns true if
  // there is a next value. Use the keyid(), salt() and rs() methods to access
  // the key-value pairs included in the header value.
  bool GetNext();

  const std::string& keyid() const {
    return keyid_;
  }

  const std::string& salt() const {
    return salt_;
  }

  uint64_t rs() const {
    return rs_;
  }

 private:
  net::HttpUtil::ValuesIterator iterator_;

  std::string keyid_;
  std::string salt_;
  uint64_t rs_;
};

// Iterates over a header that follows the syntax of the Crypto-Key HTTP header
// per the Encrypted Content-Encoding for HTTP draft. This header follows the
// #list syntax from the extended ABNF syntax defined in section 1.2 of RFC7230.
//
// https://tools.ietf.org/html/draft-thomson-http-encryption#section-4
// https://tools.ietf.org/html/rfc7230#section-1.2
class CryptoKeyHeaderIterator {
 public:
  CryptoKeyHeaderIterator(std::string::const_iterator header_begin,
                          std::string::const_iterator header_end);
  ~CryptoKeyHeaderIterator();

  // Advances the iterator to the next header value, if any. Returns true if
  // there is a next value. Use the keyid(), aesgcm128() and dh() methods to
  // access the key-value pairs included in the header value.
  bool GetNext();

  const std::string& keyid() const {
    return keyid_;
  }

  const std::string& aesgcm128() const {
    return aesgcm128_;
  }

  const std::string& dh() const {
    return dh_;
  }

 private:
  net::HttpUtil::ValuesIterator iterator_;

  std::string keyid_;
  std::string aesgcm128_;
  std::string dh_;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_ENCRYPTION_HEADER_PARSERS_H_
