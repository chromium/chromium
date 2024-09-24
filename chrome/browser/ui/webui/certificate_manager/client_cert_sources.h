// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CLIENT_CERT_SOURCES_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CLIENT_CERT_SOURCES_H_

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"

class Profile;

std::unique_ptr<CertificateManagerPageHandler::CertSource>
CreatePlatformClientCertSource(
    mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>*
        remote_client,
    Profile* profile);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
std::unique_ptr<CertificateManagerPageHandler::CertSource>
CreateProvisionedClientCertSource(Profile* profile);
#endif

#if BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<CertificateManagerPageHandler::CertSource>
CreateExtensionsClientCertSource(Profile* profile);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ClientCertManagementAccessControls {
 public:
  enum KeyStorage {
    kSoftwareBacked,
    kHardwareBacked,
  };
  enum CertLocation {
    kUser,
    kDeviceWide,
  };

  // Creates an object that can be used to check whether management functions
  // should be allowed. Once created the object is immutable and can be
  // accessed on any thread. The object should not be cached, as the policies
  // can change during runtime, so a new object should be created before every
  // operation to confirm that the operation is allowed with the current
  // policies.
  explicit ClientCertManagementAccessControls(Profile* profile);

  // Calculates whether management, such as importing client certs, is allowed
  // for the given key storage location.
  bool IsManagementAllowed(KeyStorage key_storage) const;

  // Calculates whether changing (such as deleting) a specific client cert with
  // the given key and cert storage locations is allowed.
  bool IsChangeAllowed(KeyStorage key_storage,
                       CertLocation cert_location) const;

 private:
  const bool is_guest_;
  const bool is_kiosk_;
  const ClientCertificateManagementPermission client_cert_policy_;
};
#endif

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CLIENT_CERT_SOURCES_H_
