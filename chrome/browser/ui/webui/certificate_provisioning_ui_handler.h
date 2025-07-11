// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_PROVISIONING_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_PROVISIONING_UI_HANDLER_H_

#include <utility>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace ash::cert_provisioning {
class CertProvisioningScheduler;
}  // namespace ash::cert_provisioning

namespace chromeos {
namespace cert_provisioning {

class CertificateProvisioningUiHandler : public content::WebUIMessageHandler {
 public:
  // Creates a CertificateProvisioningUiHandler for |profile|.
  // If the |profile| should be able to see and control cert provisioning
  // processes, CertificateProvisioningUiHandler will be created with a correct
  // |cert_provisioning_interface|. Otherwise |cert_provisioning_interface| will
  // be nullptr and the related UI elements will show and do nothing.
  static std::unique_ptr<CertificateProvisioningUiHandler> CreateForProfile(
      Profile* profile);

  // The constructor is public for testing, prefer using CreateForProfile when
  // possible.
  CertificateProvisioningUiHandler(
      ash::cert_provisioning::CertProvisioningScheduler* user_scheduler,
      ash::cert_provisioning::CertProvisioningScheduler* device_scheduler);

  CertificateProvisioningUiHandler(
      const CertificateProvisioningUiHandler& other) = delete;
  CertificateProvisioningUiHandler& operator=(
      const CertificateProvisioningUiHandler& other) = delete;

  ~CertificateProvisioningUiHandler() override;

  // content::WebUIMessageHandler
  void RegisterMessages() override;


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

  // Called on updates of schedulers.
  void OnStateChanged();

  // Schedulers for the given profile.
  const raw_ptr<ash::cert_provisioning::CertProvisioningScheduler>
      user_scheduler_;
  const raw_ptr<ash::cert_provisioning::CertProvisioningScheduler>
      device_scheduler_;

  // Subscribes schedulers.
  base::CallbackListSubscription user_subscription_;
  base::CallbackListSubscription device_subscription_;

  // Keeps track of the count of UI refreshes sent to the WebUI.
  unsigned int ui_refresh_count_for_testing_ = 0;

  base::WeakPtrFactory<CertificateProvisioningUiHandler> weak_ptr_factory_{
      this};
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_PROVISIONING_UI_HANDLER_H_
