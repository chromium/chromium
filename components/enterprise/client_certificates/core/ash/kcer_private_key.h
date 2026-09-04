// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ASH_KCER_PRIVATE_KEY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ASH_KCER_PRIVATE_KEY_H_

#include <optional>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/kcer/kcer.h"
#include "components/enterprise/client_certificates/core/private_key.h"

namespace client_certificates {

// A PrivateKey implementation that wraps a Kcer-managed key on ChromeOS.
// The actual private key material lives in the TPM and
// is accessed via the Kcer interface.
class KcerPrivateKey : public PrivateKey {
 public:
  // `spki` is the SubjectPublicKeyInfo of the Kcer-managed key; it identifies
  // the key (a `kcer::PrivateKeyHandle` is rebuilt from it for signing) and must
  // be non-empty (the constructor CHECKs this). `kcer_task_runner` is the task
  // runner on which `kcer` must be accessed (typically the UI thread). `source`
  // must be one of the ChromeOS sources (`kChromeOsHwKey` or `kChromeOsSwKey`);
  // it encodes whether the key is hardware-backed and is exposed through the
  // base class' `GetSource()`.
  KcerPrivateKey(base::WeakPtr<kcer::Kcer> kcer,
                 kcer::PublicKeySpki spki,
                 PrivateKeySource source);

  // Binds the Kcer-managed certificate and its `KeyInfo` to this key. After
  // this call `GetSSLPrivateKey()` returns a `kcer::SSLPrivateKeyKcer` ready
  // for TLS (it populates the base class' `ssl_private_key_`). Callable from
  // either the factory load path (once a cert has already been imported) or
  // from the certificate store at cert-commit time.
  void BindCert(scoped_refptr<const kcer::Cert> cert, kcer::KeyInfo key_info);

  // PrivateKey:
  void Sign(base::span<const uint8_t> data,
            base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
                callback) const override;
  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override;
  crypto::sign::SignatureKind GetAlgorithm() const override;
  client_certificates_pb::PrivateKey ToProto() const override;
  base::DictValue ToDict() const override;
  // Returns the cert bound by BindCert(), or nullptr if none has been bound.
  scoped_refptr<net::X509Certificate> GetBoundCert() const override;

 private:
  friend class base::RefCountedThreadSafe<KcerPrivateKey>;

  ~KcerPrivateKey() override;

  base::WeakPtr<kcer::Kcer> kcer_;
  // The SubjectPublicKeyInfo of the key. Used directly for the public-key
  // accessors and to rebuild a kcer::PrivateKeyHandle when signing.
  kcer::PublicKeySpki spki_;
  // The cert bound by BindCert(), exposed via GetBoundCert() so the certificate
  // store can build a ClientIdentity without re-listing Kcer's certs. Null
  // until a cert is bound.
  scoped_refptr<net::X509Certificate> bound_cert_;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ASH_KCER_PRIVATE_KEY_H_
