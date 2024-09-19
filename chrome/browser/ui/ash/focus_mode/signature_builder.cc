// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/focus_mode/signature_builder.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/cryptohome_util.h"
#include "chromeos/ash/components/dbus/attestation/attestation.pb.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "crypto/sha2.h"

namespace {

// Requests are signed with an ECC key using the SHA 256 digest algorithm. If
// this changes, update the string according to RFC-9421.
const char kAlgorithm[] = "ecdsa-p256-sha256";

}  // namespace

SignatureBuilder::SignatureBuilder(CertificateManager* certificate_manager)
    : certificate_manager_(certificate_manager) {}

SignatureBuilder::~SignatureBuilder() = default;

SignatureBuilder& SignatureBuilder::SetPayload(std::vector<uint8_t> bytes) {
  CHECK(!bytes.empty());
  payload_ = std::move(bytes);
  return *this;
}

SignatureBuilder& SignatureBuilder::SetBrand(std::string_view brand) {
  CHECK(!brand.empty());
  brand_ = std::string(brand);
  return *this;
}

SignatureBuilder& SignatureBuilder::SetModel(std::string_view model) {
  CHECK(!model.empty());
  model_ = std::string(model);
  return *this;
}

SignatureBuilder& SignatureBuilder::SetSoftwareVersion(
    std::string_view version) {
  CHECK(!version.empty());
  version_ = std::string(version);
  return *this;
}

SignatureBuilder& SignatureBuilder::SetDeviceId(std::string_view device_id) {
  CHECK(!device_id.empty());
  device_id_ = std::string(device_id);
  return *this;
}

bool SignatureBuilder::BuildHeaders(HeaderCallback callback) {
  if (payload_.empty()) {
    LOG(ERROR) << "Cannot sign empty payload";
    return false;
  }

  if (brand_.empty()) {
    LOG(ERROR) << "Brand must not be empty";
    return false;
  }

  if (model_.empty()) {
    LOG(ERROR) << "Model must not be empty";
    return false;
  }

  if (version_.empty()) {
    LOG(ERROR) << "Version must not be empty";
    return false;
  }

  if (device_id_.empty()) {
    LOG(ERROR) << "Device Id must not be empty";
    return false;
  }

  LOG(WARNING) << "Start certificate retrieval";
  if (!certificate_manager_->GetCertificate(
          /*force_update=*/false,
          base::BindOnce(&SignatureBuilder::OnCertificateRetrieved,
                         weak_factory_.GetWeakPtr(), std::move(callback)))) {
    return false;
  }

  return true;
}

void SignatureBuilder::OnCertificateRetrieved(
    HeaderCallback callback,
    const std::optional<CertificateManager::Key>& certificate_key) {
  if (!certificate_key.has_value()) {
    LOG(ERROR) << "Retrieving certifiate failed";
    std::move(callback).Run({});
    return;
  }

  LOG(WARNING) << "Certificate expiration " << certificate_key->expiration;

  Fields fields;
  fields.device_info = DeviceInfo();
  fields.payload_digest = PayloadDigest();
  fields.signature_params = SignatureParams();

  CertificateManager::SigningCallback signing_callback = base::BindOnce(
      &SignatureBuilder::OnBaseSigned, weak_factory_.GetWeakPtr(),
      std::move(callback), std::move(fields));
  std::string payload = SignatureBase(fields.device_info, fields.payload_digest,
                                      fields.signature_params);
  CertificateManager::CertificateResult status = certificate_manager_->Sign(
      *certificate_key, std::move(payload), std::move(signing_callback));
  if (status == CertificateManager::CertificateResult::kDisallowedByPolicy) {
    LOG(WARNING) << "Could not sign payload";
    std::move(callback).Run({});
    return;
  }

  if (status == CertificateManager::CertificateResult::kCertificateExpired) {
    // Force a certificate update.
    if (!certificate_manager_->GetCertificate(
            /*force_update=*/true,
            base::BindOnce(&SignatureBuilder::OnCertificateRetrieved,
                           weak_factory_.GetWeakPtr(), std::move(callback)))) {
      // Refreshing the certificate failed. Run the callback.
      std::move(callback).Run({});
    }
    return;
  }
}

std::string SignatureBuilder::DeviceInfoHeader() const {
  return base::StrCat({"Device-Info: ", DeviceInfo()});
}

void SignatureBuilder::OnBaseSigned(
    HeaderCallback callback,
    const Fields& fields,
    bool success,
    const std::string& signature,
    const std::string& client_certificate,
    const std::vector<std::string>& intermediate_certificates) {
  if (!success) {
    LOG(ERROR) << "Signing failed";
    return;
  }

  std::vector<std::string> headers;

  headers.push_back(base::StrCat({"Device-Info: ", fields.device_info}));
  headers.push_back(base::StrCat({"Content-Digest: ", fields.payload_digest}));
  headers.push_back(base::StrCat({"Client-Cert: ", client_certificate}));

  if (intermediate_certificates.empty()) {
    DVLOG(0) << "Certificate chain is empty. Omitting header";
  } else {
    LOG(WARNING) << "Intermeidate certificates "
                 << intermediate_certificates.size();
    headers.push_back(
        base::StrCat({"Client-Cert-Chain: ",
                      base::JoinString(intermediate_certificates, ",")}));
  }

  headers.push_back(
      base::StrCat({"Signature-Input: ", "sig=", fields.signature_params}));

  headers.push_back(base::StrCat({"Signature: ", "sig=", signature}));

  std::move(callback).Run(std::move(headers));
}

std::string SignatureBuilder::SignatureBase(
    std::string_view device_info,
    std::string_view content_digest,
    std::string_view signature_params) const {
  return base::StrCat({"\"device-info\": ", device_info, "\n",
                       "\"content-digest\": ", content_digest, "\n",
                       "\"@signature-params\": ", signature_params});
}

std::string SignatureBuilder::PayloadDigest() const {
  std::array<uint8_t, crypto::kSHA256Length> hash =
      crypto::SHA256Hash(payload_);
  return base::StrCat({"sha256=:", base::Base64Encode(hash), ":"});
}

std::string SignatureBuilder::DeviceInfo() const {
  std::vector<std::string_view> fields = {"brand=",
                                          brand_,
                                          "; ",
                                          "model=",
                                          model_,
                                          "; ",
                                          "software_version=",
                                          version_,
                                          "; ",
                                          "device_id=",
                                          device_id_};

  return base::StrCat(fields);
}

std::string SignatureBuilder::SignatureParams() const {
  base::TimeDelta time_since_epoch =
      base::Time::Now() - base::Time::UnixEpoch();
  std::string unix_time = base::NumberToString(time_since_epoch.InSeconds());

  std::vector<std::string_view> params = {
      "(\"device-info\" \"content-digest\");",
      "created=",
      unix_time,
      ";",
      "alg=",
      "\"",
      kAlgorithm,
      "\""};
  return base::StrCat(params);
}

SignatureBuilder::Fields::Fields() = default;
SignatureBuilder::Fields::Fields(const SignatureBuilder::Fields&) = default;
SignatureBuilder::Fields::~Fields() = default;
