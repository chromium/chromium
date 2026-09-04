// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ash/kcer_private_key.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chromeos/ash/components/kcer/kcer.h"
#include "chromeos/ash/components/kcer/ssl_private_key_kcer.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "crypto/sign.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"

namespace client_certificates {

namespace {

// Receives the sign result, unwraps the signature or logs the error, and
// runs the user callback.
void OnSigned(
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback,
    base::expected<kcer::Signature, kcer::Error> sign_result) {
  std::optional<std::vector<uint8_t>> result;
  if (sign_result.has_value()) {
    result = std::move(sign_result->value());
  } else {
    LOG(ERROR) << "Kcer signing failed with error: "
               << static_cast<int>(sign_result.error());
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace

KcerPrivateKey::KcerPrivateKey(base::WeakPtr<kcer::Kcer> kcer,
                               kcer::PublicKeySpki spki,
                               PrivateKeySource source)
    : PrivateKey(source, /*ssl_private_key=*/nullptr),
      kcer_(std::move(kcer)),
      spki_(std::move(spki)) {
  CHECK(source == PrivateKeySource::kChromeOsHwKey ||
        source == PrivateKeySource::kChromeOsSwKey);
  // GetSubjectPublicKeyInfo(), ToProto() and ToDict() return the SPKI, and
  // signing rebuilds a handle from it, so it must be non-empty.
  CHECK(!spki_.value().empty());
}

KcerPrivateKey::~KcerPrivateKey() = default;

void KcerPrivateKey::BindCert(scoped_refptr<const kcer::Cert> cert,
                              kcer::KeyInfo key_info) {
  CHECK(cert);
  // Retain the X509 cert so GetBoundCert() can hand it to the certificate
  // store, which would otherwise re-list Kcer's certs to recover it.
  bound_cert_ = cert->GetX509Cert();
  // A Kcer-managed cert and its KeyInfo are available, build the TLS
  // surface and hand it to the base class. GetSSLPrivateKey() returns this.
  // Before BindCert(), `ssl_private_key_` is nullptr, so the TLS
  // surface is unusable; Sign() (used for CSR upload) still works because
  // it only needs the PublicKey. The KeyInfo is consumed here and not retained:
  // only `ssl_private_key_` needs it.
  ssl_private_key_ = base::MakeRefCounted<kcer::SSLPrivateKeyKcer>(
      kcer_, std::move(cert), key_info.key_type,
      base::flat_set<kcer::SigningScheme>(
          key_info.supported_signing_schemes.begin(),
          key_info.supported_signing_schemes.end()));
}

scoped_refptr<net::X509Certificate> KcerPrivateKey::GetBoundCert() const {
  return bound_cert_;
}

void KcerPrivateKey::Sign(
    base::span<const uint8_t> data,
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback)
    const {
  if (!kcer_) {
    OnSigned(std::move(callback),
             base::unexpected(kcer::Error::kTokenIsNotAvailable));
    return;
  }

  // Rebuild a handle from the SPKI on the user token. Managed client-cert keys
  // are always generated and looked up on kcer::Token::kUser (see
  // KcerPrivateKeyFactory), we scope the handle to that token to stay
  // consistent with the rest of the load/generate flow.
  kcer_->Sign(kcer::PrivateKeyHandle(kcer::Token::kUser, spki_),
              kcer::SigningScheme::kEcdsaSecp256r1Sha256,
              kcer::DataToSign(std::vector<uint8_t>(data.begin(), data.end())),
              base::BindOnce(&OnSigned, std::move(callback)));
}

std::vector<uint8_t> KcerPrivateKey::GetSubjectPublicKeyInfo() const {
  return spki_.value();
}

crypto::sign::SignatureKind KcerPrivateKey::GetAlgorithm() const {
  return crypto::sign::ECDSA_SHA256;
}

client_certificates_pb::PrivateKey KcerPrivateKey::ToProto() const {
  client_certificates_pb::PrivateKey private_key;
  private_key.set_source(ToProtoKeySource(source_));
  // Store the SPKI bytes so the key can be re-loaded from Kcer later.
  const std::vector<uint8_t>& spki = spki_.value();
  private_key.set_wrapped_key(std::string(spki.begin(), spki.end()));
  // Hardware-backed state is encoded in `source_`, set above.
  return private_key;
}

base::DictValue KcerPrivateKey::ToDict() const {
  // Hardware-backed state is encoded in the source written by
  // BuildSerializedPrivateKey().
  return BuildSerializedPrivateKey(spki_.value());
}

}  // namespace client_certificates
