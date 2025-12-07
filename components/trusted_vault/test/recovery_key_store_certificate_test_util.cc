// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/test/recovery_key_store_certificate_test_util.h"

#include <string>

#include "base/time/time.h"

namespace trusted_vault::test_certs {

std::string_view GetCertificate() {
  return std::string_view(std::begin(kSigXml) + 3615,
                          std::begin(kSigXml) + 5363);
}

std::string_view GetSignature() {
  return std::string_view(std::begin(kSigXml) + 5387,
                          std::begin(kSigXml) + 6071);
}

std::string_view GetIntermediate1() {
  return std::string_view(std::begin(kCertXml) + 354,
                          std::begin(kCertXml) + 2082);
}

std::string_view GetIntermediate2() {
  return std::string_view(std::begin(kCertXml) + 2100,
                          std::begin(kCertXml) + 3848);
}

std::string_view GetEndpoint1() {
  return std::string_view(std::begin(kCertXml) + 3899,
                          std::begin(kCertXml) + 5007);
}

std::string_view GetEndpoint2() {
  return std::string_view(std::begin(kCertXml) + 5025,
                          std::begin(kCertXml) + 6133);
}

std::string_view GetEndpoint3() {
  return std::string_view(std::begin(kCertXml) + 6151,
                          std::begin(kCertXml) + 7259);
}

}  // namespace trusted_vault::test_certs
