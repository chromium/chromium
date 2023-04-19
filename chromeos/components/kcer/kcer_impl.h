// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_KCER_KCER_IMPL_H_
#define CHROMEOS_COMPONENTS_KCER_KCER_IMPL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_runner.h"
#include "chromeos/components/kcer/kcer.h"
#include "chromeos/components/kcer/kcer_token.h"
#include "net/cert/x509_certificate.h"

namespace kcer::internal {

// Implementation of the Kcer interface.
class KcerImpl : public Kcer {
 public:
  KcerImpl(scoped_refptr<base::TaskRunner> token_task_runner,
           base::WeakPtr<KcerToken> user_token,
           base::WeakPtr<KcerToken> device_token);
  ~KcerImpl() override;

  // Implements Kcer.
  base::CallbackListSubscription AddObserver(
      base::RepeatingClosure callback) override;
  void GenerateRsaKey(Token token,
                      uint32_t modulus_length_bits,
                      bool hardware_backed,
                      GenerateKeyCallback callback) override;
  void GenerateEcKey(Token token,
                     EllipticCurve curve,
                     bool hardware_backed,
                     GenerateKeyCallback callback) override;
  void ImportKey(Token token,
                 Pkcs8PrivateKeyInfoDer pkcs8_private_key_info_der,
                 ImportKeyCallback callback) override;
  void ImportCertFromBytes(Token token,
                           CertDer cert_der,
                           StatusCallback callback) override;
  void ImportX509Cert(Token token,
                      scoped_refptr<net::X509Certificate> cert,
                      StatusCallback callback) override;
  void ImportPkcs12Cert(Token token,
                        Pkcs12Blob pkcs12_blob,
                        std::string password,
                        bool hardware_backed,
                        StatusCallback callback) override;
  void ExportPkcs12Cert(scoped_refptr<const Cert> cert,
                        ExportPkcs12Callback callback) override;
  void RemoveKeyAndCerts(PrivateKeyHandle key,
                         StatusCallback callback) override;
  void RemoveCert(scoped_refptr<const Cert> cert,
                  StatusCallback callback) override;
  void ListKeys(base::flat_set<Token> tokens,
                ListKeysCallback callback) override;
  void ListCerts(base::flat_set<Token> tokens,
                 ListCertsCallback callback) override;
  void DoesPrivateKeyExist(PrivateKeyHandle key,
                           DoesKeyExistCallback callback) override;
  void Sign(PrivateKeyHandle key,
            SigningScheme signing_scheme,
            DataToSign data,
            SignCallback callback) override;
  void SignRsaPkcs1Raw(PrivateKeyHandle key,
                       DigestWithPrefix digest_with_prefix,
                       SignCallback callback) override;
  base::flat_set<Token> GetAvailableTokens() override;
  void GetTokenInfo(Token token, GetTokenInfoCallback callback) override;
  void GetKeyInfo(PrivateKeyHandle key, GetKeyInfoCallback callback) override;
  void SetKeyNickname(PrivateKeyHandle key,
                      std::string nickname,
                      StatusCallback callback) override;
  void SetKeyPermissions(PrivateKeyHandle key,
                         chaps::KeyPermissions key_permissions,
                         StatusCallback callback) override;
  void SetCertProvisioningProfileId(PrivateKeyHandle key,
                                    std::string profile_id,
                                    StatusCallback callback) override;

 private:
  base::WeakPtr<KcerToken>& GetToken(Token token);

  // Tast runner for the tokens. Can be nullptr if no tokens are available to
  // the current Kcer instance.
  scoped_refptr<base::TaskRunner> token_task_runner_;
  // Pointers to kcer tokens. Can contain nullptr-s if a token is not available
  // to the current instance of Kcer. All requests to the tokens must be posted
  // on the `token_task_runner_`. The pointers themself also belong to the
  // `token_task_runner_`'s sequence and can only be used within KcerImpl in a
  // very limited way (consult documentation for WeakPtr for details).
  base::WeakPtr<KcerToken> user_token_;
  base::WeakPtr<KcerToken> device_token_;

  base::RepeatingCallbackList<void()> observers_;

  base::WeakPtrFactory<KcerImpl> weak_factory_{this};
};

}  // namespace kcer::internal

#endif  // CHROMEOS_COMPONENTS_KCER_KCER_IMPL_H_
