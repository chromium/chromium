// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/kcer/kcer_impl.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_runner.h"
#include "base/types/expected.h"
#include "chromeos/components/kcer/kcer.h"
#include "chromeos/components/kcer/kcer_token.h"
#include "chromeos/components/kcer/token_results_merger.h"
#include "net/cert/x509_certificate.h"

namespace kcer::internal {

KcerImpl::KcerImpl(scoped_refptr<base::TaskRunner> token_task_runner,
                   base::WeakPtr<KcerToken> user_token,
                   base::WeakPtr<KcerToken> device_token)
    : token_task_runner_(std::move(token_task_runner)),
      user_token_(std::move(user_token)),
      device_token_(std::move(device_token)) {}

KcerImpl::~KcerImpl() = default;

base::CallbackListSubscription KcerImpl::AddObserver(
    base::RepeatingClosure callback) {
  // TODO(244408716): Implement.
  return {};
}

void KcerImpl::GenerateRsaKey(Token token,
                              uint32_t modulus_length_bits,
                              bool hardware_backed,
                              GenerateKeyCallback callback) {
  const base::WeakPtr<KcerToken>& kcer_token = GetToken(token);
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::GenerateRsaKey, kcer_token,
                     modulus_length_bits, hardware_backed,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::GenerateEcKey(Token token,
                             EllipticCurve curve,
                             bool hardware_backed,
                             GenerateKeyCallback callback) {
  const base::WeakPtr<KcerToken>& kcer_token = GetToken(token);
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::GenerateEcKey, kcer_token, curve,
                     hardware_backed,
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::ImportKey(Token token,
                         Pkcs8PrivateKeyInfoDer pkcs8_private_key_info_der,
                         ImportKeyCallback callback) {
  const base::WeakPtr<KcerToken>& kcer_token = GetToken(token);
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::ImportKey, kcer_token,
                     std::move(pkcs8_private_key_info_der),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::ImportCertFromBytes(Token token,
                                   CertDer cert_der,
                                   StatusCallback callback) {
  const base::WeakPtr<KcerToken>& kcer_token = GetToken(token);
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::ImportCertFromBytes, kcer_token,
                     std::move(cert_der),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::ImportX509Cert(Token token,
                              scoped_refptr<net::X509Certificate> cert,
                              StatusCallback callback) {
  if (!cert) {
    return std::move(callback).Run(
        base::unexpected(Error::kInvalidCertificate));
  }

  const CRYPTO_BUFFER* buffer = cert->cert_buffer();
  CertDer cert_der(std::vector<uint8_t>(
      CRYPTO_BUFFER_data(buffer),
      CRYPTO_BUFFER_data(buffer) + CRYPTO_BUFFER_len(buffer)));

  return ImportCertFromBytes(token, std::move(cert_der), std::move(callback));
}

void KcerImpl::ImportPkcs12Cert(Token token,
                                Pkcs12Blob pkcs12_blob,
                                std::string password,
                                bool hardware_backed,
                                StatusCallback callback) {
  // TODO(244408716): Implement.
}

void KcerImpl::ExportPkcs12Cert(scoped_refptr<const Cert> cert,
                                ExportPkcs12Callback callback) {
  // TODO(244408716): Implement.
}

void KcerImpl::RemoveKeyAndCerts(PrivateKeyHandle key,
                                 StatusCallback callback) {
  // TODO(244408716): Implement.
}

void KcerImpl::RemoveCert(scoped_refptr<const Cert> cert,
                          StatusCallback callback) {
  if (!cert) {
    return std::move(callback).Run(
        base::unexpected(Error::kInvalidCertificate));
  }

  const base::WeakPtr<KcerToken>& kcer_token = GetToken(cert->GetToken());
  if (!kcer_token.MaybeValid()) {
    return std::move(callback).Run(
        base::unexpected(Error::kTokenIsNotAvailable));
  }
  token_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerToken::RemoveCert, kcer_token, std::move(cert),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}

void KcerImpl::ListKeys(base::flat_set<Token> tokens,
                        ListKeysCallback callback) {
  // TODO(244408716): Implement.
}

void KcerImpl::ListCerts(base::flat_set<Token> tokens,
                         ListCertsCallback callback) {
  if (tokens.empty()) {
    return std::move(callback).Run(/*certs=*/{}, /*errors=*/{});
  }

  scoped_refptr<TokenResultsMerger<scoped_refptr<const Cert>>> merger =
      TokenResultsMerger<scoped_refptr<const Cert>>::Create(
          /*results_to_receive=*/tokens.size(), std::move(callback));
  for (Token token : tokens) {
    auto callback_for_token = merger->GetCallback(token);
    const base::WeakPtr<KcerToken>& kcer_token = GetToken(token);
    if (!kcer_token.MaybeValid()) {
      std::move(callback_for_token)
          .Run(base::unexpected(Error::kTokenIsNotAvailable));
    } else {
      token_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&KcerToken::ListCerts, kcer_token,
                                    base::BindPostTaskToCurrentDefault(
                                        std::move(callback_for_token))));
    }
  }
}

void KcerImpl::DoesPrivateKeyExist(PrivateKeyHandle key, DoesKeyExistCallback) {
  // TODO(244408716): Implement.
}

void KcerImpl::Sign(PrivateKeyHandle key,
                    SigningScheme signing_scheme,
                    DataToSign data,
                    SignCallback callback) {
  // TODO(244408716): Implement.
}

void KcerImpl::SignRsaPkcs1Raw(PrivateKeyHandle key,
                               DigestWithPrefix digest_with_prefix,
                               SignCallback callback) {
  // TODO(244408716): Implement.
}

base::flat_set<Token> KcerImpl::GetAvailableTokens() {
  base::flat_set<Token> result;
  if (user_token_.MaybeValid()) {
    result.insert(Token::kUser);
  }
  if (device_token_.MaybeValid()) {
    result.insert(Token::kDevice);
  }
  return result;
}

void KcerImpl::GetTokenInfo(Token token, GetTokenInfoCallback callback) {
  // TODO(244408716): Implement.
}

void KcerImpl::GetKeyInfo(PrivateKeyHandle key, GetKeyInfoCallback callback) {
  // TODO(244408716): Implement.
}

void KcerImpl::SetKeyNickname(PrivateKeyHandle key,
                              std::string nickname,
                              StatusCallback callback) {
  // TODO(244408716): Implement.
}

void KcerImpl::SetKeyPermissions(PrivateKeyHandle key,
                                 chaps::KeyPermissions key_permissions,
                                 StatusCallback callback) {
  // TODO(244408716): Implement.
}

void KcerImpl::SetCertProvisioningProfileId(PrivateKeyHandle key,
                                            std::string profile_id,
                                            StatusCallback callback) {
  // TODO(244408716): Implement.
}

base::WeakPtr<internal::KcerToken>& KcerImpl::GetToken(Token token) {
  switch (token) {
    case Token::kUser:
      return user_token_;
    case Token::kDevice:
      return device_token_;
  }
}

}  // namespace kcer::internal
