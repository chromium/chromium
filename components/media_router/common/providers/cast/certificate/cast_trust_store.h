// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_TRUST_STORE_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_TRUST_STORE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"

namespace bssl {
class TrustStore;
class TrustStore;
class ParsedCertificate;
}  // namespace bssl
namespace cast_certificate {
namespace testing {
class ScopedCastTrustStoreConfig;
}  // namespace testing

class CastTrustStore {
 public:
  using AccessCallback = base::OnceCallback<void(bssl::TrustStore*)>;

  CastTrustStore(const CastTrustStore&) = delete;
  CastTrustStore(CastTrustStore&&) = delete;
  CastTrustStore& operator=(const CastTrustStore&) = delete;
  CastTrustStore& operator=(CastTrustStore&&) = delete;

  static void AccessInstance(AccessCallback callback);

 private:
  friend class base::NoDestructor<CastTrustStore>;
  friend class cast_certificate::testing::ScopedCastTrustStoreConfig;

  // GetInstance() provides access to the trust store singleton `instance`.
  // `instance` is constructed at the first call to the function. Callers should
  // run with `may_block`=false (or unspecified), if calling from an IO thread
  // to let the constructor defer loading of a developer certificate path to
  // another thread.
  static CastTrustStore* GetInstance(bool may_block = false);

  explicit CastTrustStore(bool may_block);

  // AddDefaultCertificates() is called by the constructor. It adds the built-in
  // certificates to `store_`. If developer certificates are specified on the
  // command line, then they are loaded from file.
  void AddDefaultCertificates(bool may_block);

  // Adds the built-in Chrome certificates from memory.
  void AddBuiltInCertificates();

  // Adding developer certificates may need to be done off of the IO thread due
  // to blocking file access.
  void NonBlockingAddCertificateFromPath(base::FilePath cert_path);

  // Loads certificates from `cert_path` and adds them to the trust store.
  void AddCertificateFromPath(base::FilePath cert_path);

  // Clears certificates in `store_`. For use in testing.
  void ClearCertificatesForTesting();

  // Adds a trust anchor from a parsed certificate.
  void AddAnchor(std::shared_ptr<const bssl::ParsedCertificate> cert);

  base::Lock lock_;
  bssl::TrustStoreInMemory store_ GUARDED_BY(lock_);
};

}  // namespace cast_certificate

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_CAST_TRUST_STORE_H_
