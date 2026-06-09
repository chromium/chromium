// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_internals_utils.h"

#include "base/base64url.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/enterprise/browser/reporting/report_request.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "crypto/sha2.h"

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

namespace enterprise_connectors::utils {

namespace {

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
    case client_certificates::PrivateKeySource::kOsSoftwareKey:
      return connectors_internals::mojom::KeyTrustLevel::OS_SOFTWARE;
    case client_certificates::PrivateKeySource::kAndroidKey:
      return connectors_internals::mojom::KeyTrustLevel::HW;
    case client_certificates::PrivateKeySource::kChromeOsHwKey:
      return connectors_internals::mojom::KeyTrustLevel::HW;
    case client_certificates::PrivateKeySource::kChromeOsSwKey:
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

  const auto spki_bytes = private_key->GetSubjectPublicKeyInfo();
  return connectors_internals::mojom::LoadedKeyInfo::New(
      ConvertPrivateKeySource(private_key->GetSource()),
      AlgorithmToType(private_key->GetAlgorithm()),
      HashAndEncodeString(BufferToString(spki_bytes)), std::move(upload_status),
      static_cast<bool>(private_key->GetSSLPrivateKey()));
}

connectors_internals::mojom::CertificateMetadataPtr ConvertCertificate(
    scoped_refptr<net::X509Certificate> certificate) {
  if (!certificate) {
    return nullptr;
  }

  return connectors_internals::mojom::CertificateMetadata::New(
      base::HexEncodeLower(certificate->serial_number()),
      base::HexEncodeLower(certificate->CalculateChainFingerprint256()),
      base::UnlocalizedTimeFormatWithPattern(certificate->valid_start(),
                                             "MMM d, yyyy"),
      base::UnlocalizedTimeFormatWithPattern(certificate->valid_expiry(),
                                             "MMM d, yyyy"),
      certificate->subject().GetDisplayName(),
      certificate->issuer().GetDisplayName());
}
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

}  // namespace

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

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

connectors_internals::mojom::ClientIdentityPtr GetIdentity(
    client_certificates::CertificateProvisioningService* provisioning_service,
    std::vector<std::string>& enabled_levels,
    const std::string& enabled_level) {
  const auto& status = provisioning_service->GetCurrentStatus();
  if (!(status.is_policy_enabled)) {
    return nullptr;
  }
  enabled_levels.push_back(enabled_level);

  if (!status.identity.has_value()) {
    return nullptr;
  }

  return ConvertIdentity(status.identity.value(), status.last_upload_code);
}

connectors_internals::mojom::ClientIdentityPtr ConvertIdentity(
    const client_certificates::ClientIdentity& identity,
    const std::optional<client_certificates::HttpCodeOrClientError>&
        key_upload_code) {
  return connectors_internals::mojom::ClientIdentity::New(
      identity.name, ConvertPrivateKey(identity.private_key, key_upload_code),
      ConvertCertificate(identity.certificate));
}

#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

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

std::string GetJsonForReportRequest(
    const enterprise_reporting::ReportRequest& request) {
  auto proto_request = request.GetChromeProfileReportRequest();

  int policy_count = 0;
  if (proto_request.has_browser_report()) {
    for (const auto& profile_info :
         proto_request.browser_report().chrome_user_profile_infos()) {
      policy_count += profile_info.chrome_policies_size();
    }
  }

  if (proto_request.has_attestation_payload() &&
      !proto_request.attestation_payload().attestation_blob().empty()) {
    proto_request.mutable_attestation_payload()->set_attestation_blob(
        "[attestation blob collected but omitted for readability]");
  }

  std::string signals_json =
      enterprise_reporting::GetSecuritySignalsInReport(proto_request);

  if (policy_count > 0) {
    std::optional<base::Value> parsed_value =
        base::JSONReader::Read(signals_json, base::JSON_PARSE_RFC);

    if (parsed_value && parsed_value->is_dict()) {
      parsed_value->GetDict().Set(
          "chrome_policies",
          base::StringPrintf(
              "[%d policies collected but omitted for readability]",
              policy_count));

      base::JSONWriter::WriteWithOptions(
          *parsed_value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &signals_json);
    }
  }

  return signals_json;
}

}  // namespace enterprise_connectors::utils
