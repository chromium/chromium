// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_PROVISIONING_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_PROVISIONING_UI_HANDLER_H_

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/cert_provisioning.mojom.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace chromeos {
namespace cert_provisioning {

class CertificateProvisioningUiHandler
    : public content::WebUIMessageHandler,
      public crosapi::mojom::CertProvisioningObserver {
 public:
  // Creates a CertificateProvisioningUiHandler for |profile|.
  // If the |profile| should be able to see and control cert provisioning
  // processes, CertificateProvisioningUiHandler will be created with a correct
  // |cert_provisioning_interface|. Otherwise |cert_provisioning_interface| will
  // be nullptr and the related UI elements will show and do nothing.
  static std::unique_ptr<CertificateProvisioningUiHandler> CreateForProfile(
      Profile* profile);

  // |cert_provisioning_interface| is the mojo interface to communicate with the
  // cert provisioning component, can be nullptr.
  // The constructor is public for testing, prefer using CreateForProfile when
  // possible.
  explicit CertificateProvisioningUiHandler(
      crosapi::mojom::CertProvisioning* cert_provisioning_interface);

  CertificateProvisioningUiHandler(
      const CertificateProvisioningUiHandler& other) = delete;
  CertificateProvisioningUiHandler& operator=(
      const CertificateProvisioningUiHandler& other) = delete;

  ~CertificateProvisioningUiHandler() override;

  // content::WebUIMessageHandler
  void RegisterMessages() override;

  // crosapi::mojom::CertProvisioningObserver
  void OnStateChanged() override;

  // For testing: Reads the count of UI refreshes sent to the WebUI (since
  // instantiation or the last call to this function) and resets it to 0.
  unsigned int ReadAndResetUiRefreshCountForTesting();

 private:
  // Send the list of certificate provisioning processes to the UI, triggered by
  // the UI when it loads.
  // |args| is expected to be empty.
  void HandleRefreshCertificateProvisioningProcesses(
      const base::Value::List& args);

  // Trigger an update / refresh on a certificate provisioning process.
  // |args| is expected to contain two arguments:
  // The argument at index 0 is a string specifying the certificate profile id
  // of the process that an update should be triggered for. The argument at
  // index 1 is a boolean specifying whether the process is a user-specific
  // (false) or a device-wide (true) certificate provisioning process.
  void HandleTriggerCertificateProvisioningProcessUpdate(
      const base::Value::List& args);

  // Triggers a reset to a particular certificate provisioning process.
  // |args| is expected to contain two arguments:
  // The argument at index 0 is a string specifying the certificate profile id
  // of the process that an update should be triggered for. The argument at
  // index 1 is a boolean specifying whether the process is a user-specific
  // (false) or a device-wide (true) certificate provisioning process.
  void HandleTriggerCertificateProvisioningProcessReset(
      const base::Value::List& args);

  // Send the list of certificate provisioning processes to the UI.
  void RefreshCertificateProvisioningProcesses();

  // Called when the status of cert provisioning processes is received.
  void GotStatus(
      std::vector<crosapi::mojom::CertProvisioningProcessStatusPtr> status);

  // The interface to communicate with the cert provisioning component.
  // Can be null (e.g. for non-primary / non-main profiles), in which case this
  // part of the UI should show and do nothing.
  const raw_ptr<crosapi::mojom::CertProvisioning> cert_provisioning_interface_;
  // Receives mojo messages about cert provisioning updates.
  mojo::Receiver<crosapi::mojom::CertProvisioningObserver> receiver_{this};

  // Keeps track of the count of UI refreshes sent to the WebUI.
  unsigned int ui_refresh_count_for_testing_ = 0;

  base::WeakPtrFactory<CertificateProvisioningUiHandler> weak_ptr_factory_{
      this};
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_PROVISIONING_UI_HANDLER_H_
