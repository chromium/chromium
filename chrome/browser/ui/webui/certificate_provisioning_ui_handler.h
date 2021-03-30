// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_PROVISIONING_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_PROVISIONING_UI_HANDLER_H_

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace chromeos {
namespace cert_provisioning {

class CertificateProvisioningUiHandler
    : public content::WebUIMessageHandler,
      public ash::cert_provisioning::CertProvisioningSchedulerObserver {
 public:
  // Creates a CertificateProvisioningUiHandler for |user_profile|, which uses:
  // (*) The CertProvisioningScheduler associated with |user_profile|, if any.
  // (*) The device-wide CertProvisioningScheduler, if it exists and the
  //     |user_profile| is affiliated.
  static std::unique_ptr<CertificateProvisioningUiHandler> CreateForProfile(
      Profile* user_profile);

  // The constructed CertificateProvisioningUiHandler will use
  // |scheduler_for_user| to list certificate provisioning processes that belong
  // to the user, and |scheduler_for_device|, to list certificatge provisioning
  // processes that are device-wide. Both can be nullptr. Note: Intended to be
  // called directly for testing. Use CreateForProfile in production code
  // instead.
  // |user_profile| is used to determine if the current user is affiliated and
  // decide if |scheduler_for_device| should be used based on that. This pattern
  // is useful for unit-testing the affiliation detection logic.
  CertificateProvisioningUiHandler(
      Profile* user_profile,
      ash::cert_provisioning::CertProvisioningScheduler* scheduler_for_user,
      ash::cert_provisioning::CertProvisioningScheduler* scheduler_for_device);

  CertificateProvisioningUiHandler(
      const CertificateProvisioningUiHandler& other) = delete;
  CertificateProvisioningUiHandler& operator=(
      const CertificateProvisioningUiHandler& other) = delete;

  ~CertificateProvisioningUiHandler() override;

  // content::WebUIMessageHandler.
  void RegisterMessages() override;

  // CertProvisioningSchedulerObserver:
  void OnVisibleStateChanged() override;

  // For testing: Reads the count of UI refreshes sent to the WebUI (since
  // instantiation or the last call to this function) and resets it to 0.
  unsigned int ReadAndResetUiRefreshCountForTesting();

 private:
  // Send the list of certificate provisioning processes to the UI, triggered by
  // the UI when it loads.
  // |args| is expected to be empty.
  void HandleRefreshCertificateProvisioningProcesses(
      const base::ListValue* args);

  // Trigger an update / refresh on a certificate provisioning process.
  // |args| is expected to contain two arguments:
  // The argument at index 0 is a string specifying the certificate profile id
  // of the process that an update should be triggered for. The argument at
  // index 1 is a boolean specifying whether the process is a user-specific
  // (false) or a device-wide (true) certificate provisioning process.
  void HandleTriggerCertificateProvisioningProcessUpdate(
      const base::ListValue* args);

  // Send the list of certificate provisioning processes to the UI.
  void RefreshCertificateProvisioningProcesses();

  // Called when the |hold_back_updates_timer_| expires.
  void OnHoldBackUpdatesTimerExpired();

  // Returns true if device-wide certificate provisioning processes should be
  // displayed, i.e. if the |user_profile| is affiliated.
  static bool ShouldUseDeviceWideProcesses(Profile* user_profile);

  // The user-specific CertProvisioningScheduler. Can be nullptr.
  // Unowned.
  ash::cert_provisioning::CertProvisioningScheduler* const scheduler_for_user_;

  // The device-wide CertProvisioningScheduler. Can be nullptr.
  // Unowned.
  ash::cert_provisioning::CertProvisioningScheduler* const
      scheduler_for_device_;

  // When this timer is running, updates provided by the schedulers should not
  // be forwarded to the UI until it fires. Used to prevent spamming the UI if
  // many events come in in rapid succession.
  base::OneShotTimer hold_back_updates_timer_;

  // When this is true, an update should be sent to the UI when
  // |hold_back_updates_timer_| fires.
  bool update_after_hold_back_ = false;

  // Keeps track of the count of UI refreshes sent to the WebUI.
  unsigned int ui_refresh_count_for_testing_ = 0;

  // Keeps track of the CertProvisioningSchedulers that this UI handler
  // observes.
  base::ScopedMultiSourceObservation<
      ash::cert_provisioning::CertProvisioningScheduler,
      ash::cert_provisioning::CertProvisioningSchedulerObserver>
      observed_schedulers_{this};

  base::WeakPtrFactory<CertificateProvisioningUiHandler> weak_ptr_factory_{
      this};
};

}  // namespace cert_provisioning
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_PROVISIONING_UI_HANDLER_H_
