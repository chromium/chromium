// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_FOCUS_MODE_SIGNATURE_BUILDER_H_
#define CHROME_BROWSER_UI_ASH_FOCUS_MODE_SIGNATURE_BUILDER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/ash/focus_mode/certificate_manager.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "components/account_id/account_id.h"

// Constructs a signature base and headers for YMC API requests to satisfy
// request signing requirements.
// https://developers.google.com/youtube/mediaconnect/guides/device_attestation
class SignatureBuilder {
 public:
  explicit SignatureBuilder(CertificateManager* certificate_manager);

  SignatureBuilder(const SignatureBuilder&) = delete;
  SignatureBuilder& operator=(const SignatureBuilder&) = delete;

  ~SignatureBuilder();

  SignatureBuilder& SetAccountId(const AccountId& account_id);

  SignatureBuilder& SetPayload(std::vector<uint8_t> bytes);

  // Device Info fields
  SignatureBuilder& SetBrand(std::string_view brand);
  SignatureBuilder& SetModel(std::string_view model);
  SignatureBuilder& SetSoftwareVersion(std::string_view version);
  SignatureBuilder& SetDeviceId(std::string_view device_id);

  using HeaderCallback =
      base::OnceCallback<void(const std::vector<std::string>& headers)>;
  bool BuildHeaders(HeaderCallback callback);

  using SignatureBaseCallback =
      base::OnceCallback<void(const std::string& signature_base)>;
  bool BuildSignatureBase(SignatureBaseCallback callback);

  // Returns a formatted 'Device-Info' HTTP headers string.
  std::string DeviceInfoHeader() const;

 private:
  using SignedCallback =
      base::OnceCallback<void(const std::vector<uint8_t>& signature)>;
  void SignSignatureBase(const std::string& signature_base,
                         SignedCallback callback);

  void OnCertificateRetrieved(
      HeaderCallback callback,
      const std::optional<CertificateManager::Key>& key);

  struct Fields {
    Fields();
    Fields(const Fields&);
    ~Fields();

    std::string device_info;
    std::string payload_digest;
    std::string signature_params;
  };

  void OnBaseSigned(HeaderCallback callback,
                    const Fields& fields,
                    bool success,
                    const std::string& signature,
                    const std::string& client_certificate,
                    const std::vector<std::string>& intermediate_certificates);

  std::string SignatureBase(std::string_view device_info,
                            std::string_view content_digest,
                            std::string_view signature_params) const;

  // Returns the sha-256 of `payload_` encoded in lowercase hexadecimal.
  std::string PayloadDigest() const;

  std::string DeviceInfo() const;

  std::string SignatureParams() const;

  // Retrieves a certificate and signs the signature base.
  raw_ptr<CertificateManager> certificate_manager_;

  std::optional<AccountId> account_id_;

  std::vector<uint8_t> payload_;

  std::string brand_;
  std::string model_;
  std::string version_;
  std::string device_id_;

  base::WeakPtrFactory<SignatureBuilder> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_FOCUS_MODE_SIGNATURE_BUILDER_H_
