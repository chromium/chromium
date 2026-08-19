// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ASH_KCER_PRIVATE_KEY_FACTORY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ASH_KCER_PRIVATE_KEY_FACTORY_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/kcer/kcer.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"

namespace client_certificates {

class KcerPrivateKey;

// PrivateKeyFactory implementation for ChromeOS that generates and loads
// keys via Kcer. Keys are stored in the user's Chaps PKCS#11 slot (TPM-backed
// when available, with software fallback).
class KcerPrivateKeyFactory : public PrivateKeyFactory {
 public:
  // `kcer` is a weak pointer to the Kcer instance (must be accessed on
  // the UI thread where Kcer lives).
  // (typically the UI thread).
  explicit KcerPrivateKeyFactory(base::WeakPtr<kcer::Kcer> kcer);

  ~KcerPrivateKeyFactory() override;

  // PrivateKeyFactory:
  void CreatePrivateKey(PrivateKeyCallback callback) override;
  void LoadPrivateKey(
      const client_certificates_pb::PrivateKey& serialized_private_key,
      PrivateKeyCallback callback) override;
  void LoadPrivateKeyFromDict(const base::DictValue& serialized_private_key,
                              PrivateKeyCallback callback) override;

 private:
  void OnKeyGenerated(PrivateKeyCallback callback,
                      base::expected<kcer::PublicKey, kcer::Error> result);

  void OnHardwareKeyGenerationError(PrivateKeyCallback callback,
                                    kcer::Error error);

  void OnSoftwareKeyGenerated(
      PrivateKeyCallback callback,
      base::expected<kcer::PublicKey, kcer::Error> result);

  // Tags the freshly generated `public_key` as owned by the CA Connector flow,
  // then builds and delivers a KcerPrivateKey via OnGeneratedKeyInfo. Takes no
  // PrivateKeySource - that's resolved from the key itself further down.
  void DeliverGeneratedKey(PrivateKeyCallback callback,
                           kcer::PublicKey public_key);

  // Continuation of DeliverGeneratedKey, invoked once the ownership tag write
  // completes. A tag failure is non-blocking (only cleanup/audit metadata is
  // missing); reads back the key's KeyInfo to check if it's hardware backed.
  void OnBrowserEnterpriseClientCertTagSet(
      PrivateKeyCallback callback,
      kcer::PublicKeySpki spki,
      base::expected<void, kcer::Error> result);

  // Tail of the generation chain: maps the generated key's KeyInfo onto
  // kChromeOsHwKey / kChromeOsSwKey and delivers the KcerPrivateKey. A KeyInfo
  // failure resolves to software rather than aborting - the key still
  // generated.
  void OnGeneratedKeyInfo(PrivateKeyCallback callback,
                          kcer::PublicKeySpki spki,
                          base::expected<kcer::KeyInfo, kcer::Error> key_info);

  // Common tail of LoadPrivateKey / LoadPrivateKeyFromDict. Both public methods
  // only extract the source and the key from their respective serialization
  // format and hand them here. This validates that `source` is a supported
  // ChromeOS source, verifies the key still exists in Kcer, and binds any
  // matching certificate. `source` is std::nullopt when the format carried an
  // unrecognized value; that (and a missing Kcer) resolves to a null key.
  void LoadPrivateKeyImpl(std::optional<PrivateKeySource> source,
                          kcer::PublicKeySpki spki,
                          PrivateKeyCallback callback);

  // Continuations of the load chain: GetKeyInfo confirms the key is usable
  // (any error aborts the load) and feeds the cert lookup/BindCert call.
  // OnGotKeyInfo also reconciles `source` against the key's actual
  // hardware-backing state; see the .cc for why this can be out of sync.
  void OnGotKeyInfo(PrivateKeyCallback callback,
                    kcer::PublicKeySpki spki,
                    PrivateKeySource source,
                    base::expected<kcer::KeyInfo, kcer::Error> key_info);

  void OnListedCerts(PrivateKeyCallback callback,
                     kcer::PublicKeySpki spki,
                     PrivateKeySource source,
                     kcer::KeyInfo key_info,
                     std::vector<scoped_refptr<const kcer::Cert>> certs,
                     base::flat_map<kcer::Token, kcer::Error> errors);

  base::WeakPtr<kcer::Kcer> kcer_;
  base::WeakPtrFactory<KcerPrivateKeyFactory> weak_factory_{this};
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ASH_KCER_PRIVATE_KEY_FACTORY_H_
