// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/dm_client.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/enterprise_companion_status.h"
#include "chrome/enterprise_companion/enterprise_companion_version.h"
#include "components/policy/core/common/cloud/client_data_delegate.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_companion {

namespace {

class DMConfiguration : public policy::DeviceManagementService::Configuration {
 public:
  DMConfiguration() = default;
  ~DMConfiguration() override = default;

  std::string GetDMServerUrl() const override {
    return "https://m.google.com/devicemanagement/data/api";
  }
  std::string GetAgentParameter() const override {
    return base::StrCat(
        {"ChromeEnterpriseCompanion ", kEnterpriseCompanionVersion});
  }
  std::string GetPlatformParameter() const override {
    int32_t major = 0;
    int32_t minor = 0;
    int32_t bugfix = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    return base::StringPrintf(
        "%s|%s|%d.%d.%d", base::SysInfo::OperatingSystemName().c_str(),
        base::SysInfo::OperatingSystemArchitecture().c_str(), major, minor,
        bugfix);
  }
  std::string GetRealtimeReportingServerUrl() const override {
    return "https://chromereporting-pa.googleapis.com/v1/events";
  }
  std::string GetEncryptedReportingServerUrl() const override {
    return "https://chromereporting-pa.googleapis.com/v1/record";
  }
  std::string GetReportingConnectorServerUrl(
      content::BrowserContext* context) const override {
    return std::string();
  }
};

class ClientDataDelegate : public policy::ClientDataDelegate {
 public:
  ClientDataDelegate() = default;
  ~ClientDataDelegate() override = default;

  void FillRegisterBrowserRequest(
      enterprise_management::RegisterBrowserRequest* request,
      base::OnceClosure callback) const override {
    request->set_machine_name(policy::GetMachineName());
    request->set_os_platform(policy::GetOSPlatform());
    request->set_os_version(policy::GetOSVersion());
    request->set_allocated_browser_device_identifier(
        policy::GetBrowserDeviceIdentifier().release());
    std::move(callback).Run();
  }
};

// Interface to a CloudPolicyClient which interacts with the device management
// server. May perform blocking IO.
class DMClientImpl : public DMClient, policy::CloudPolicyClient::Observer {
 public:
  explicit DMClientImpl(
      std::unique_ptr<policy::DeviceManagementService::Configuration> config,
      CloudPolicyClientProvider cloud_policy_client_provider,
      scoped_refptr<device_management_storage::DMStorage> dm_storage)
      : dm_service_(std::move(config)),
        cloud_policy_client_(
            std::move(cloud_policy_client_provider).Run(&dm_service_)),
        dm_storage_(dm_storage) {
    dm_service_.ScheduleInitialization(0);
    cloud_policy_client_->AddObserver(this);
  }
  ~DMClientImpl() override { cloud_policy_client_->RemoveObserver(this); }

  // Overrides for DMClient.
  void RegisterBrowser(
      base::OnceCallback<void(const EnterpriseCompanionStatus&)> callback)
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!pending_callback_);

    if (ShouldSkipRegistration()) {
      std::move(callback).Run(EnterpriseCompanionStatus::Success());
      return;
    }

    dm_storage_->RemoveAllPolicies();
    pending_callback_ = std::move(callback);
    cloud_policy_client_->RegisterBrowserWithEnrollmentToken(
        dm_storage_->GetEnrollmentToken(), dm_storage_->GetDeviceID(),
        client_data_delegate_, dm_storage_->IsEnrollmentMandatory());
  }

  // Overrides for policy::CloudPolicyClient::Observer.
  void OnPolicyFetched(policy::CloudPolicyClient*) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << __func__;
  }

  void OnRegistrationStateChanged(policy::CloudPolicyClient*) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << __func__;
    if (pending_callback_) {
      std::move(pending_callback_)
          .Run(EnterpriseCompanionStatus::FromDeviceManagementStatus(
              cloud_policy_client_->last_dm_status()));
    }
    if (cloud_policy_client_->is_registered()) {
      dm_storage_->StoreDmToken(cloud_policy_client_->dm_token());
    }
  }

  void OnClientError(policy::CloudPolicyClient*) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << __func__;
    if (pending_callback_) {
      std::move(pending_callback_)
          .Run(EnterpriseCompanionStatus::FromDeviceManagementStatus(
              cloud_policy_client_->last_dm_status()));
    }
    if (cloud_policy_client_->last_dm_status() ==
        policy::DM_STATUS_SERVICE_DEVICE_NEEDS_RESET) {
      dm_storage_->DeleteDMToken();
    }
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  policy::DeviceManagementService dm_service_;
  std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client_;
  scoped_refptr<device_management_storage::DMStorage> dm_storage_;
  ClientDataDelegate client_data_delegate_;
  base::OnceCallback<void(const EnterpriseCompanionStatus&)> pending_callback_;

  bool ShouldSkipRegistration() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (dm_storage_->GetEnrollmentToken().empty()) {
      VLOG(1) << "Registration skipped: device not managed.";
      return true;
    } else if (!dm_storage_->GetDmToken().empty()) {
      VLOG(1) << "Registration skipped: device already registered.";
      return true;
    }
    return false;
  }
};

}  // namespace

CloudPolicyClientProvider GetDefaultCloudPolicyClientProvider(
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_shared_url_loader_factory) {
  return base::BindOnce(
      [](std::unique_ptr<network::PendingSharedURLLoaderFactory>
             pending_shared_url_loader_factory,
         policy::DeviceManagementService* dm_service) {
        return std::make_unique<policy::CloudPolicyClient>(
            dm_service, network::SharedURLLoaderFactory::Create(
                            std::move(pending_shared_url_loader_factory)));
      },
      std::move(pending_shared_url_loader_factory));
}

std::unique_ptr<policy::DeviceManagementService::Configuration>
CreateDeviceManagementServiceConfig() {
  return std::make_unique<DMConfiguration>();
}

std::unique_ptr<DMClient> CreateDMClient(
    CloudPolicyClientProvider cloud_policy_client_provider,
    scoped_refptr<device_management_storage::DMStorage> dm_storage,
    std::unique_ptr<policy::DeviceManagementService::Configuration> config) {
  return std::make_unique<DMClientImpl>(
      std::move(config), std::move(cloud_policy_client_provider), dm_storage);
}

}  // namespace enterprise_companion
