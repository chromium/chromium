// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ASH_KCER_PRIVATE_KEY_FACTORY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ASH_KCER_PRIVATE_KEY_FACTORY_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
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
  // `kcer_task_runner`). `kcer_task_runner` is the sequence where Kcer lives
  // (typically the UI thread).
  KcerPrivateKeyFactory(
      base::WeakPtr<kcer::Kcer> kcer,
      scoped_refptr<base::SequencedTaskRunner> kcer_task_runner);

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

  void OnHardwareKeyFailed(PrivateKeyCallback callback, kcer::Error error);

  void OnSoftwareKeyGenerated(
      PrivateKeyCallback callback,
      base::expected<kcer::PublicKey, kcer::Error> result);

  // Tags the freshly generated `public_key` as owned by the browser enterprise
  // client certificate (CA Connector) provisioning flow, then builds a
  // KcerPrivateKey wrapping it and returns it via `callback` from
  // OnBrowserEnterpriseClientCertTagSet.
  void DeliverGeneratedKey(PrivateKeyCallback callback,
                           kcer::PublicKey public_key,
                           PrivateKeySource source);

  // Continuation of DeliverGeneratedKey, invoked once the ownership tag write
  // completes. A tag failure is non-blocking: the key is still delivered (only
  // the cleanup/audit metadata is missing).
  void OnBrowserEnterpriseClientCertTagSet(
      PrivateKeyCallback callback,
      kcer::PublicKeySpki spki,
      PrivateKeySource source,
      base::expected<void, kcer::Error> result);

  // Common tail of LoadPrivateKey / LoadPrivateKeyFromDict. Both public methods
  // only extract the source and the key from their respective serialization
  // format and hand them here. This validates that `source` is a supported
  // ChromeOS source, verifies the key still exists in Kcer, and binds any
  // matching certificate. `source` is std::nullopt when the format carried an
  // unrecognized value; that (and a missing Kcer) resolves to a null key.
  void LoadPrivateKeyImpl(std::optional<PrivateKeySource> source,
                          kcer::PublicKeySpki spki,
                          PrivateKeyCallback callback);

  // Continuations of the load chain. GetKeyInfo both confirms the key is
  // usable (any error - missing key, malformed SPKI, transient failure -
  // aborts the load) and yields its KeyInfo (signing schemes / key type); we
  // then look up the matching certificate so we can call
  // KcerPrivateKey::BindCert before returning.
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
  scoped_refptr<base::SequencedTaskRunner> kcer_task_runner_;
  base::WeakPtrFactory<KcerPrivateKeyFactory> weak_factory_{this};
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_ASH_KCER_PRIVATE_KEY_FACTORY_H_
