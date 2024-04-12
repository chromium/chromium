// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CERTIFICATE_PROVISIONING_SERVICE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CERTIFICATE_PROVISIONING_SERVICE_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/upload_client_error.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace client_certificates {

class ContextDelegate;
class CertificateStore;
class KeyUploadClient;

// Service in charge with provisioning a managed identity (private key and
// certificate).
class CertificateProvisioningService : public KeyedService {
 public:
  using GetManagedIdentityCallback =
      base::OnceCallback<void(std::optional<ClientIdentity>)>;

  // Status that can be used to view the current provisioning state and
  // loaded identity.
  struct Status {
    explicit Status(bool is_provisioning);

    Status(const Status&);
    Status& operator=(const Status&);

    ~Status();

    // Whether the service is still currently provisioning the identity.
    bool is_provisioning{};

    // Whether the certificate provisioning policy is enabled at this level.
    bool is_policy_enabled{};

    // Cached identity.
    std::optional<ClientIdentity> identity = std::nullopt;

    // HTTP response code, or client-side error, for the last upload request.
    std::optional<HttpCodeOrClientError> last_upload_code = std::nullopt;
  };

  ~CertificateProvisioningService() override;

  static std::unique_ptr<CertificateProvisioningService> Create(
      PrefService* profile_prefs,
      CertificateStore* certificate_store,
      std::unique_ptr<ContextDelegate> context_delegate,
      std::unique_ptr<KeyUploadClient> upload_client);

  // Will invoke `callback` with the managed identity once it has been
  // successfully loaded and the policies for its usage are enabled as well.
  // Otherwise, run it with std::nullopt. If the identity failed to load for
  // some reason, subsequent calls will retry loading it.
  virtual void GetManagedIdentity(GetManagedIdentityCallback callback) = 0;

  // Returns metadata about the current status of the service, mainly for
  // debugging purposes.
  virtual Status GetCurrentStatus() const = 0;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CERTIFICATE_PROVISIONING_SERVICE_H_
