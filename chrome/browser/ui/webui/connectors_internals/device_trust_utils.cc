// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/connectors_internals/device_trust_utils.h"

#include "build/build_config.h"
#include "components/enterprise/buildflags/buildflags.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "base/base64url.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_MAC)
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "net/cert/x509_certificate.h"
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

namespace enterprise_connectors::utils {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

connectors_internals::mojom::KeyTrustLevel ParseTrustLevel(
    BPKUR::KeyTrustLevel trust_level) {
  switch (trust_level) {
    case BPKUR::CHROME_BROWSER_HW_KEY:
      return connectors_internals::mojom::KeyTrustLevel::HW;
    case BPKUR::CHROME_BROWSER_OS_KEY:
      return connectors_internals::mojom::KeyTrustLevel::OS;
    default:
      return connectors_internals::mojom::KeyTrustLevel::UNSPECIFIED;
  }
}

connectors_internals::mojom::KeyType AlgorithmToType(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm) {
  switch (algorithm) {
    case crypto::SignatureVerifier::RSA_PKCS1_SHA1:
    case crypto::SignatureVerifier::RSA_PKCS1_SHA256:
    case crypto::SignatureVerifier::RSA_PSS_SHA256:
      return connectors_internals::mojom::KeyType::RSA;
    case crypto::SignatureVerifier::ECDSA_SHA256:
      return connectors_internals::mojom::KeyType::EC;
  }
}

connectors_internals::mojom::KeyManagerPermanentFailure ConvertPermanentFailure(
    std::optional<DeviceTrustKeyManager::PermanentFailure> permanent_failure) {
  if (!permanent_failure) {
    return connectors_internals::mojom::KeyManagerPermanentFailure::UNSPECIFIED;
  }

  switch (permanent_failure.value()) {
    case DeviceTrustKeyManager::PermanentFailure::kCreationUploadConflict:
      return connectors_internals::mojom::KeyManagerPermanentFailure::
          CREATION_UPLOAD_CONFLICT;
    case DeviceTrustKeyManager::PermanentFailure::kInsufficientPermissions:
      return connectors_internals::mojom::KeyManagerPermanentFailure::
          INSUFFICIENT_PERMISSIONS;
    case DeviceTrustKeyManager::PermanentFailure::kOsRestriction:
      return connectors_internals::mojom::KeyManagerPermanentFailure::
          OS_RESTRICTION;
    case DeviceTrustKeyManager::PermanentFailure::kInvalidInstallation:
      return connectors_internals::mojom::KeyManagerPermanentFailure::
          INVALID_INSTALLATION;
  }
}

std::string HashAndEncodeString(const std::string& spki_bytes) {
  std::string encoded_string;
  base::Base64UrlEncode(crypto::SHA256HashString(spki_bytes),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_string);
  return encoded_string;
}

connectors_internals::mojom::Int32ValuePtr ToMojomValue(
    std::optional<int> integer_value) {
  return integer_value ? connectors_internals::mojom::Int32Value::New(
                             integer_value.value())
                       : nullptr;
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

std::string BufferToString(base::span<const uint8_t> buffer) {
  return std::string(buffer.begin(), buffer.end());
}

connectors_internals::mojom::KeyTrustLevel ConvertPrivateKeySource(
    client_certificates::PrivateKeySource source) {
  switch (source) {
    case client_certificates::PrivateKeySource::kUnexportableKey:
      return connectors_internals::mojom::KeyTrustLevel::HW;
    case client_certificates::PrivateKeySource::kSoftwareKey:
      return connectors_internals::mojom::KeyTrustLevel::OS;
  }
}

connectors_internals::mojom::LoadedKeyInfoPtr ConvertPrivateKey(
    scoped_refptr<client_certificates::PrivateKey> private_key,
    const std::optional<client_certificates::HttpCodeOrClientError>&
        key_upload_code) {
  if (!private_key) {
    return nullptr;
  }

  connectors_internals::mojom::KeyUploadStatusPtr upload_status = nullptr;
  if (key_upload_code.has_value() && key_upload_code->has_value()) {
    upload_status =
        connectors_internals::mojom::KeyUploadStatus::NewSyncKeyResponseCode(
            ToMojomValue(key_upload_code->value()));
  }

  if (key_upload_code.has_value() && !key_upload_code->has_value()) {
    upload_status =
        connectors_internals::mojom::KeyUploadStatus::NewUploadClientError(
            std::string(client_certificates::UploadClientErrorToString(
                key_upload_code->error())));
  }

  const auto& spki_bytes = private_key->GetSubjectPublicKeyInfo();
  return connectors_internals::mojom::LoadedKeyInfo::New(
      ConvertPrivateKeySource(private_key->GetSource()),
      AlgorithmToType(private_key->GetAlgorithm()),
      HashAndEncodeString(BufferToString(spki_bytes)),
      std::move(upload_status));
}

connectors_internals::mojom::CertificateMetadataPtr ConvertCertificate(
    scoped_refptr<net::X509Certificate> certificate) {
  if (!certificate) {
    return nullptr;
  }

  return connectors_internals::mojom::CertificateMetadata::New(
      base::ToLowerASCII(base::HexEncode(certificate->serial_number().data(),
                                         certificate->serial_number().size())),
      base::ToLowerASCII(
          base::HexEncode(certificate->CalculateChainFingerprint256().data)),
      base::UnlocalizedTimeFormatWithPattern(certificate->valid_start(),
                                             "MMM d, yyyy"),
      base::UnlocalizedTimeFormatWithPattern(certificate->valid_expiry(),
                                             "MMM d, yyyy"),
      certificate->subject().GetDisplayName(),
      certificate->issuer().GetDisplayName());
}

#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

}  // namespace

connectors_internals::mojom::KeyInfoPtr GetKeyInfo() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  auto* key_manager = g_browser_process->browser_policy_connector()
                          ->chrome_browser_cloud_management_controller()
                          ->GetDeviceTrustKeyManager();
  if (key_manager) {
    auto metadata = key_manager->GetLoadedKeyMetadata();
    if (metadata) {
      if (!metadata->spki_bytes.empty()) {
        // A key was loaded successfully.
        return connectors_internals::mojom::KeyInfo::New(
            connectors_internals::mojom::KeyManagerInitializedValue::KEY_LOADED,
            connectors_internals::mojom::LoadedKeyInfo::New(
                ParseTrustLevel(metadata->trust_level),
                AlgorithmToType(metadata->algorithm),
                HashAndEncodeString(metadata->spki_bytes),
                connectors_internals::mojom::KeyUploadStatus::
                    NewSyncKeyResponseCode(
                        ToMojomValue(metadata->synchronization_response_code))),
            ConvertPermanentFailure(metadata->permanent_failure));
      }

      return connectors_internals::mojom::KeyInfo::New(
          connectors_internals::mojom::KeyManagerInitializedValue::NO_KEY,
          nullptr, ConvertPermanentFailure(metadata->permanent_failure));
    }

    return connectors_internals::mojom::KeyInfo::New(
        connectors_internals::mojom::KeyManagerInitializedValue::NO_KEY,
        nullptr,
        connectors_internals::mojom::KeyManagerPermanentFailure::UNSPECIFIED);
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return connectors_internals::mojom::KeyInfo::New(
      connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED,
      nullptr,
      connectors_internals::mojom::KeyManagerPermanentFailure::UNSPECIFIED);
}

bool CanDeleteDeviceTrustKey() {
#if BUILDFLAG(IS_MAC)
  version_info::Channel channel = chrome::GetChannel();
  return channel != version_info::Channel::STABLE &&
         channel != version_info::Channel::BETA;
#else
  // Unsupported on non-Mac platforms.
  return false;
#endif  // BUILDFLAG(IS_MAC)
}

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

connectors_internals::mojom::ClientIdentityPtr ConvertIdentity(
    const client_certificates::ClientIdentity& identity,
    const std::optional<client_certificates::HttpCodeOrClientError>&
        key_upload_code) {
  return connectors_internals::mojom::ClientIdentity::New(
      identity.name, ConvertPrivateKey(identity.private_key, key_upload_code),
      ConvertCertificate(identity.certificate));
}

#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

}  // namespace enterprise_connectors::utils
