// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ANDROID_PRIVATE_KEY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ANDROID_PRIVATE_KEY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/enterprise/client_certificates/android/browser_binding/browser_key.h"
#include "components/enterprise/client_certificates/android/browser_binding/browser_key_android.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/proto/client_certificates_database.pb.h"

namespace client_certificates {

class AndroidPrivateKey : public PrivateKey {
 public:
  // Wraps browser`key` and associates it with PrivateKeySource::kAndroidKey.
  explicit AndroidPrivateKey(std::unique_ptr<BrowserKey> key);

  // PrivateKey:
  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) const override;
  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override;
  crypto::SignatureVerifier::SignatureAlgorithm GetAlgorithm() const override;
  client_certificates_pb::PrivateKey ToProto() const override;
  base::Value::Dict ToDict() const override;

  // Returns the security level of the key.
  BrowserKey::SecurityLevel GetSecurityLevel() const;

 private:
  friend class base::RefCountedThreadSafe<BrowserKeyAndroid>;
  friend class base::RefCountedThreadSafe<BrowserKey>;

  ~AndroidPrivateKey() override;

  std::unique_ptr<BrowserKey> key_;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ANDROID_PRIVATE_KEY_H_
