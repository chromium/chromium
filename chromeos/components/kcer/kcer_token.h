// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_KCER_KCER_TOKEN_H_
#define CHROMEOS_COMPONENTS_KCER_KCER_TOKEN_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chromeos/components/kcer/kcer.h"
#include "chromeos/components/kcer/key_permissions.pb.h"

namespace kcer::internal {

// Provides an interface for a single PKCS#11 token, that actually performs
// storage of keys and certificates. This class is an implementation detail of
// Kcer and should only be used by implementations of the Kcer interface. Each
// KcerToken can be used by multiple instances of Kcer interface. Each instance
// of Kcer interface can use multiple KcerToken-s.
// KcerToken-s are expected to exist on a non-UI thread. Passed `callback`-s
// must already be bound to the correct task runner (see base::BindPostTask).
class COMPONENT_EXPORT(KCER) KcerToken {
 public:
  using TokenListKeysCallback =
      base::OnceCallback<void(base::expected<std::vector<PublicKey>, Error>)>;
  using TokenListCertsCallback = base::OnceCallback<void(
      base::expected<std::vector<scoped_refptr<const Cert>>, Error>)>;

  KcerToken() = default;
  KcerToken(const KcerToken&) = delete;
  KcerToken& operator=(const KcerToken&) = delete;
  KcerToken(KcerToken&&) = delete;
  KcerToken& operator=(Token&&) = delete;
  virtual ~KcerToken() = default;

  // These methods mirror the methods from the Kcer class, except they are
  // specialized for a single token.
  virtual void GenerateRsaKey(uint32_t modulus_length_bits,
                              bool hardware_backed,
                              Kcer::GenerateKeyCallback callback) = 0;
  virtual void GenerateEcKey(EllipticCurve curve,
                             bool hardware_backed,
                             Kcer::GenerateKeyCallback callback) = 0;
  virtual void ImportKey(Pkcs8PrivateKeyInfoDer pkcs8_private_key_info_der,
                         Kcer::ImportKeyCallback callback) = 0;
  virtual void ImportCertFromBytes(CertDer cert_der,
                                   Kcer::StatusCallback callback) = 0;
  virtual void ImportPkcs12Cert(Pkcs12Blob pkcs12_blob,
                                std::string password,
                                bool hardware_backed,
                                Kcer::StatusCallback callback) = 0;
  virtual void ExportPkcs12Cert(scoped_refptr<const Cert> cert,
                                Kcer::ExportPkcs12Callback callback) = 0;
  virtual void RemoveKeyAndCerts(PrivateKeyHandle key,
                                 Kcer::StatusCallback callback) = 0;
  virtual void RemoveCert(scoped_refptr<const Cert> cert,
                          Kcer::StatusCallback callback) = 0;
  virtual void ListKeys(TokenListKeysCallback callback) = 0;
  virtual void ListCerts(TokenListCertsCallback callback) = 0;
  virtual void DoesPrivateKeyExist(PrivateKeyHandle key,
                                   Kcer::DoesKeyExistCallback callback) = 0;
  virtual void Sign(PrivateKeyHandle key,
                    SigningScheme signing_scheme,
                    DataToSign data,
                    Kcer::SignCallback callback) = 0;
  virtual void SignRsaPkcs1Raw(PrivateKeyHandle key,
                               SigningScheme signing_scheme,
                               DigestWithPrefix digest_with_prefix,
                               Kcer::SignCallback callback) = 0;
  virtual void GetTokenInfo(Kcer::GetTokenInfoCallback callback) = 0;
  virtual void GetKeyInfo(PrivateKeyHandle key,
                          Kcer::GetKeyInfoCallback callback) = 0;
  virtual void SetKeyNickname(PrivateKeyHandle key,
                              std::string nickname,
                              Kcer::StatusCallback callback) = 0;
  virtual void SetKeyPermissions(PrivateKeyHandle key,
                                 chaps::KeyPermissions key_permissions,
                                 Kcer::StatusCallback callback) = 0;
  virtual void SetCertProvisioningProfileId(PrivateKeyHandle key,
                                            std::string profile_id,
                                            Kcer::StatusCallback callback) = 0;
};

}  // namespace kcer::internal

#endif  // CHROMEOS_COMPONENTS_KCER_KCER_TOKEN_H_
