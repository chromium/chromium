// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ash/kcer_private_key_factory.h"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/kcer/kcer.h"
#include "components/enterprise/client_certificates/core/ash/kcer_private_key.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

namespace client_certificates {

namespace {

// Returns the SubjectPublicKeyInfo bytes of `cert`, or an empty vector if the
// cert is missing or its SPKI cannot be extracted.
std::vector<uint8_t> ExtractCertSpki(const kcer::Cert& cert) {
  scoped_refptr<net::X509Certificate> x509 = cert.GetX509Cert();
  if (!x509) {
    return {};
  }
  std::string_view cert_der =
      net::x509_util::CryptoBufferAsStringPiece(x509->cert_buffer());
  std::string_view spki_piece;
  if (!net::asn1::ExtractSPKIFromDERCert(cert_der, &spki_piece)) {
    return {};
  }
  return std::vector<uint8_t>(spki_piece.begin(), spki_piece.end());
}

}  // namespace

KcerPrivateKeyFactory::KcerPrivateKeyFactory(
    base::WeakPtr<kcer::Kcer> kcer,
    scoped_refptr<base::SequencedTaskRunner> kcer_task_runner)
    : kcer_(std::move(kcer)), kcer_task_runner_(std::move(kcer_task_runner)) {
  CHECK(kcer_task_runner_);
}

KcerPrivateKeyFactory::~KcerPrivateKeyFactory() = default;

void KcerPrivateKeyFactory::CreatePrivateKey(PrivateKeyCallback callback) {
  if (!kcer_) {
    std::move(callback).Run(nullptr);
    return;
  }
  // Try to generate a hardware-backed EC P-256 key first.
  kcer_->GenerateEcKey(
      kcer::Token::kUser, kcer::EllipticCurve::kP256,
      /*hardware_backed=*/true,
      base::BindOnce(&KcerPrivateKeyFactory::OnKeyGenerated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KcerPrivateKeyFactory::LoadPrivateKey(
    const client_certificates_pb::PrivateKey& serialized_private_key,
    PrivateKeyCallback callback) {
  // Hardware-backed state is encoded in the source itself.
  const std::string& wrapped_key_str = serialized_private_key.wrapped_key();
  LoadPrivateKeyImpl(
      ToPrivateKeySource(serialized_private_key.source()),
      kcer::PublicKeySpki(
          std::vector<uint8_t>(wrapped_key_str.begin(), wrapped_key_str.end())),
      std::move(callback));
}

void KcerPrivateKeyFactory::LoadPrivateKeyFromDict(
    const base::DictValue& serialized_private_key,
    PrivateKeyCallback callback) {
  std::optional<int> source = serialized_private_key.FindInt(kKeySource);
  const std::string* encoded_spki = serialized_private_key.FindString(kKey);
  std::string decoded_spki;
  if (!source.has_value() || !encoded_spki ||
      !base::Base64Decode(*encoded_spki, &decoded_spki)) {
    std::move(callback).Run(nullptr);
    return;
  }
  // Hardware-backed state is encoded in the source itself.
  LoadPrivateKeyImpl(
      ToPrivateKeySource(*source),
      kcer::PublicKeySpki(
          std::vector<uint8_t>(decoded_spki.begin(), decoded_spki.end())),
      std::move(callback));
}

void KcerPrivateKeyFactory::OnKeyGenerated(
    PrivateKeyCallback callback,
    base::expected<kcer::PublicKey, kcer::Error> result) {
  if (result.has_value()) {
    DeliverGeneratedKey(std::move(callback), std::move(result.value()),
                        PrivateKeySource::kChromeOsHwKey);
    return;
  }
  // Hardware-backed key generation failed. Fall back to software key.
  OnHardwareKeyFailed(std::move(callback), result.error());
}

void KcerPrivateKeyFactory::OnHardwareKeyFailed(PrivateKeyCallback callback,
                                                kcer::Error error) {
  // TODO(crbug.com/517117656): Convert this log into a histogram so we can
  // track how often hardware-backed key generation falls back to software at
  // an aggregate level.
  LOG(WARNING) << "Hardware-backed key generation failed (error: "
               << static_cast<int>(error) << "), falling back to software key.";
  if (!kcer_) {
    std::move(callback).Run(nullptr);
    return;
  }
  kcer_->GenerateEcKey(
      kcer::Token::kUser, kcer::EllipticCurve::kP256,
      /*hardware_backed=*/false,
      base::BindOnce(&KcerPrivateKeyFactory::OnSoftwareKeyGenerated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KcerPrivateKeyFactory::OnSoftwareKeyGenerated(
    PrivateKeyCallback callback,
    base::expected<kcer::PublicKey, kcer::Error> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Software key generation also failed (error: "
               << static_cast<int>(result.error()) << ").";
    std::move(callback).Run(nullptr);
    return;
  }
  DeliverGeneratedKey(std::move(callback), std::move(result.value()),
                      PrivateKeySource::kChromeOsSwKey);
}

void KcerPrivateKeyFactory::DeliverGeneratedKey(PrivateKeyCallback callback,
                                                kcer::PublicKey public_key,
                                                PrivateKeySource source) {
  if (!kcer_) {
    std::move(callback).Run(nullptr);
    return;
  }
  // Tag the freshly generated key as owned by the browser enterprise client
  // certificate (CA Connector) provisioning flow so the stack can identify the
  // keys it owns for later cleanup and auditing. This applies to both the
  // TPM-backed and software fallback paths, since both deliver here. The tag is
  // written immediately after key generation, as required by the Kcer API
  // contract.
  kcer::PublicKeySpki spki = public_key.GetSpki();
  auto handle = kcer::PrivateKeyHandle(kcer::Token::kUser, spki);
  kcer_->SetBrowserEnterpriseClientCertTag(
      std::move(handle),
      base::BindOnce(
          &KcerPrivateKeyFactory::OnBrowserEnterpriseClientCertTagSet,
          weak_factory_.GetWeakPtr(), std::move(callback), std::move(spki),
          source));
}

void KcerPrivateKeyFactory::OnBrowserEnterpriseClientCertTagSet(
    PrivateKeyCallback callback,
    kcer::PublicKeySpki spki,
    PrivateKeySource source,
    base::expected<void, kcer::Error> result) {
  if (!kcer_) {
    std::move(callback).Run(nullptr);
    return;
  }
  if (!result.has_value()) {
    // Non-blocking: a failure to persist the ownership tag must not abort
    // provisioning. The key itself is valid and usable; only the metadata used
    // for future cleanup/auditing is missing.
    // TODO(crbug.com/517117656): Convert this log into a histogram so we can
    // track how often tagging fails at an aggregate level.
    LOG(WARNING) << "Failed to tag browser enterprise client certificate key "
                    "(error: "
                 << static_cast<int>(result.error())
                 << "); delivering the key without the ownership tag.";
  }
  std::move(callback).Run(base::MakeRefCounted<KcerPrivateKey>(
      kcer_, std::move(spki), kcer_task_runner_, source));
}

void KcerPrivateKeyFactory::LoadPrivateKeyImpl(
    std::optional<PrivateKeySource> source,
    kcer::PublicKeySpki spki,
    PrivateKeyCallback callback) {
  if (!source.has_value() ||
      (source.value() != PrivateKeySource::kChromeOsHwKey &&
       source.value() != PrivateKeySource::kChromeOsSwKey) ||
      spki.value().empty() || !kcer_) {
    // Reject unsupported sources, empty/missing SPKI (malformed serialized
    // data) and a torn-down Kcer up front. An empty SPKI would otherwise hit
    // the non-empty CHECK in the KcerPrivateKey constructor.
    std::move(callback).Run(nullptr);
    return;
  }
  // Ask Kcer for the key's KeyInfo. This both verifies the key is usable (any
  // error - missing key, malformed stored SPKI, transient failure - aborts the
  // load in OnGotKeyInfo) and yields the key_type / supported signing schemes
  // needed to later construct a kcer::SSLPrivateKeyKcer (matches the production
  // pattern in chromeos/ash/components/kcer/client_cert_identity_kcer.cc).
  kcer_->GetKeyInfo(
      kcer::PrivateKeyHandle(kcer::Token::kUser, kcer::PublicKeySpki(spki)),
      base::BindOnce(&KcerPrivateKeyFactory::OnGotKeyInfo,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(spki), source.value()));
}

void KcerPrivateKeyFactory::OnGotKeyInfo(
    PrivateKeyCallback callback,
    kcer::PublicKeySpki spki,
    PrivateKeySource source,
    base::expected<kcer::KeyInfo, kcer::Error> key_info) {
  if (!kcer_) {
    std::move(callback).Run(nullptr);
    return;
  }
  if (!key_info.has_value()) {
    // GetKeyInfo failed: the key is gone (kKeyNotFound), the stored SPKI is
    // malformed (kFailedToGetPkcs11Id), or Kcer hit a transient error. In none
    // of these cases can we positively confirm the key exists or build its TLS
    // surface, so there is nothing to load. (Note: now that the redundant
    // DoesPrivateKeyExist pre-check is gone, a GetKeyInfo failure no longer
    // implies the key is present, so we must not return an unbound key here.)
    std::move(callback).Run(nullptr);
    return;
  }
  kcer_->ListCerts(
      base::flat_set<kcer::Token>({kcer::Token::kUser}),
      base::BindOnce(&KcerPrivateKeyFactory::OnListedCerts,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(spki), source, std::move(key_info.value())));
}

void KcerPrivateKeyFactory::OnListedCerts(
    PrivateKeyCallback callback,
    kcer::PublicKeySpki spki,
    PrivateKeySource source,
    kcer::KeyInfo key_info,
    std::vector<scoped_refptr<const kcer::Cert>> certs,
    base::flat_map<kcer::Token, kcer::Error> /*errors*/) {
  // Pass a copy of `spki` into the handle so the original stays alive for the
  // SubjectPublicKeyInfo comparison below.
  scoped_refptr<KcerPrivateKey> private_key =
      base::MakeRefCounted<KcerPrivateKey>(kcer_, spki, kcer_task_runner_,
                                           source);
  // Find the cert whose SubjectPublicKeyInfo matches our stored SPKI.
  const std::vector<uint8_t>& target_spki = spki.value();
  for (auto& cert : certs) {
    if (!cert) {
      continue;
    }
    if (ExtractCertSpki(*cert) == target_spki) {
      private_key->BindCert(std::move(cert), std::move(key_info));
      break;
    }
  }
  // If no matching cert was found the KcerPrivateKey is returned unbound:
  // SignSlowly remains usable for CSR upload, and GetSSLPrivateKey will return
  // nullptr until BindCert is called (e.g. after a fresh cert is committed by
  // KcerCertificateStore).
  std::move(callback).Run(std::move(private_key));
}

}  // namespace client_certificates
