// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_ANDROID_BROWSER_BINDING_BROWSER_KEY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_ANDROID_BROWSER_BINDING_BROWSER_KEY_H_

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/jni_zero/jni_zero.h"

namespace client_certificates {

class BrowserKey {
 public:
  // Mirrors the SecurityLevel IntDef in BrowserKey.java.
  // has to match the SecurityLevel enum in
  // tools/metrics/histograms/metadata/enterprise/enums.xml
  enum class SecurityLevel {
    kOSSoftware = 0,
    kTrustedEnvironment = 1,
    kStrongbox = 2,
    kUnknown = 3,
    kMaxValue = kUnknown,
  };

  BrowserKey() = default;
  BrowserKey(const BrowserKey&) = delete;
  BrowserKey& operator=(const BrowserKey&) = delete;
  virtual ~BrowserKey() = default;

  // Returns the identifier of this browser key as a new vector.
  virtual std::vector<uint8_t> GetIdentifier() const = 0;

  // Returns the private key of this browser key as a Java object.
  virtual jni_zero::ScopedJavaLocalRef<jobject> GetPrivateKey() const = 0;

  // Returns the public key of this browser key as a SPKI in DER format.
  virtual std::vector<uint8_t> GetPublicKeyAsSPKI() const = 0;

  // Returns the SSL private key of this browser key as a scoped refptr.
  virtual scoped_refptr<net::SSLPrivateKey> GetSSLPrivateKey() const = 0;

  // Signs the provided data with the private key of this browser key and
  // returns the signature.
  virtual std::optional<std::vector<uint8_t>> Sign(
      const std::vector<uint8_t>& data) const = 0;

  // Returns the security level of the key.
  virtual SecurityLevel GetSecurityLevel() const = 0;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_ANDROID_BROWSER_BINDING_BROWSER_KEY_H_
