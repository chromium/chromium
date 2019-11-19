// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/tpm/install_attributes.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome/tpm_util.h"
#include "components/policy/proto/install_attributes.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

InstallAttributes* g_install_attributes = nullptr;

// Calling SetForTesting sets this flag. This flag means that the production
// code which calls Initialize and Shutdown will have no effect - the test
// install attributes will remain in place until ShutdownForTesting is called.
bool g_using_install_attributes_for_testing = false;

// Number of TPM lock state query retries during consistency check.
const int kDbusRetryCount = 12;

// Interval of TPM lock state query retries during consistency check.
const int kDbusRetryIntervalInSeconds = 5;

std::string ReadMapKey(const std::map<std::string, std::string>& map,
                       const std::string& key) {
  std::map<std::string, std::string>::const_iterator entry = map.find(key);
  if (entry != map.end()) {
    return entry->second;
  }
  return std::string();
}

void WarnIfNonempty(const std::map<std::string, std::string>& map,
                    const std::string& key) {
  if (!ReadMapKey(map, key).empty()) {
    LOG(WARNING) << key << " expected to be empty.";
  }
}

// Reports the metric for whether the locking succeeded with existing locked
// attributes equal to the requested ones.
void ReportExistingLockUma(bool is_existing_lock) {
  UMA_HISTOGRAM_BOOLEAN("Enterprise.ExistingInstallAttributesLock",
                        is_existing_lock);
}

}  // namespace

// static
void InstallAttributes::Initialize() {
  // Don't reinitialize if a specific instance has already been set for test.
  if (g_using_install_attributes_for_testing)
    return;

  DCHECK(!g_install_attributes);
  g_install_attributes = new InstallAttributes(CryptohomeClient::Get());
  base::FilePath install_attrs_file;
  CHECK(base::PathService::Get(dbus_paths::FILE_INSTALL_ATTRIBUTES,
                               &install_attrs_file));
  g_install_attributes->Init(install_attrs_file);
}

// static
bool InstallAttributes::IsInitialized() {
  return g_install_attributes;
}

// static
void InstallAttributes::Shutdown() {
  if (g_using_install_attributes_for_testing)
    return;

  DCHECK(g_install_attributes);
  delete g_install_attributes;
  g_install_attributes = nullptr;
}

// static
InstallAttributes* InstallAttributes::Get() {
  DCHECK(g_install_attributes);
  return g_install_attributes;
}

// static
void InstallAttributes::SetForTesting(InstallAttributes* test_instance) {
  DCHECK(!g_install_attributes);
  DCHECK(!g_using_install_attributes_for_testing);
  g_install_attributes = test_instance;
  g_using_install_attributes_for_testing = true;
}

// static
void InstallAttributes::ShutdownForTesting() {
  DCHECK(g_using_install_attributes_for_testing);
  // Don't delete the test instance, we are not the owner.
  g_install_attributes = nullptr;
  g_using_install_attributes_for_testing = false;
}

InstallAttributes::InstallAttributes(CryptohomeClient* cryptohome_client)
    : cryptohome_client_(cryptohome_client) {}

InstallAttributes::~InstallAttributes() {}

void InstallAttributes::Init(const base::FilePath& cache_file) {
  DCHECK(!device_locked_);

  // Mark the consistency check as running to ensure that LockDevice() is
  // blocked, but wait for the cryptohome service to be available before
  // actually calling TriggerConsistencyCheck().
  consistency_check_running_ = true;
  cryptohome_client_->WaitForServiceToBeAvailable(
      base::BindOnce(&InstallAttributes::OnCryptohomeServiceInitiallyAvailable,
                     weak_ptr_factory_.GetWeakPtr()));

  if (!base::PathExists(cache_file)) {
    LOG_IF(WARNING, base::SysInfo::IsRunningOnChromeOS())
        << "Install attributes missing, first sign in";
    return;
  }

  device_locked_ = true;

  char buf[16384];
  int len = base::ReadFile(cache_file, buf, sizeof(buf));
  if (len == -1 || len >= static_cast<int>(sizeof(buf))) {
    PLOG(ERROR) << "Failed to read " << cache_file.value();
    return;
  }

  cryptohome::SerializedInstallAttributes install_attrs_proto;
  if (!install_attrs_proto.ParseFromArray(buf, len)) {
    LOG(ERROR) << "Failed to parse install attributes cache.";
    return;
  }

  google::protobuf::RepeatedPtrField<
      const cryptohome::SerializedInstallAttributes::Attribute>::iterator entry;
  std::map<std::string, std::string> attr_map;
  for (entry = install_attrs_proto.attributes().begin();
       entry != install_attrs_proto.attributes().end(); ++entry) {
    // The protobuf values contain terminating null characters, so we have to
    // sanitize the value here.
    attr_map.insert(
        std::make_pair(entry->name(), std::string(entry->value().c_str())));
  }

  DecodeInstallAttributes(attr_map);
}

void InstallAttributes::ReadImmutableAttributes(base::OnceClosure callback) {
  if (device_locked_) {
    std::move(callback).Run();
    return;
  }

  cryptohome_client_->InstallAttributesIsReady(
      base::BindOnce(&InstallAttributes::ReadAttributesIfReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void InstallAttributes::ReadAttributesIfReady(base::OnceClosure callback,
                                              base::Optional<bool> is_ready) {
  if (is_ready.value_or(false)) {
    registration_mode_ = policy::DEVICE_MODE_NOT_SET;
    if (!tpm_util::InstallAttributesIsInvalid() &&
        !tpm_util::InstallAttributesIsFirstInstall()) {
      device_locked_ = true;

      static const char* const kEnterpriseAttributes[] = {
          kAttrEnterpriseDeviceId,   kAttrEnterpriseDomain,
          kAttrEnterpriseRealm,      kAttrEnterpriseMode,
          kAttrEnterpriseOwned,      kAttrEnterpriseUser,
          kAttrConsumerKioskEnabled,
      };
      std::map<std::string, std::string> attr_map;
      for (size_t i = 0; i < base::size(kEnterpriseAttributes); ++i) {
        std::string value;
        if (tpm_util::InstallAttributesGet(kEnterpriseAttributes[i], &value))
          attr_map[kEnterpriseAttributes[i]] = value;
      }

      DecodeInstallAttributes(attr_map);
    }
  }
  std::move(callback).Run();
}

void InstallAttributes::SetBlockDevmodeInTpm(
    bool block_devmode,
    DBusMethodCallback<cryptohome::BaseReply> callback) {
  DCHECK(!callback.is_null());
  DCHECK(!device_locked_);

  cryptohome::SetFirmwareManagementParametersRequest request;
  // Set the flags, according to enum FirmwareManagementParametersFlags from
  // rpc.proto if devmode is blocked.
  if (block_devmode) {
    request.set_flags(
        cryptohome::DEVELOPER_DISABLE_BOOT |
        cryptohome::DEVELOPER_DISABLE_CASE_CLOSED_DEBUGGING_UNLOCK);
  }

  cryptohome_client_->SetFirmwareManagementParametersInTpm(request,
                                                           std::move(callback));
}

void InstallAttributes::LockDevice(policy::DeviceMode device_mode,
                                   const std::string& domain,
                                   const std::string& realm,
                                   const std::string& device_id,
                                   LockResultCallback callback) {
  CHECK((device_mode == policy::DEVICE_MODE_ENTERPRISE && !domain.empty() &&
         realm.empty() && !device_id.empty()) ||
        (device_mode == policy::DEVICE_MODE_ENTERPRISE_AD && domain.empty() &&
         !realm.empty() && !device_id.empty()) ||
        (device_mode == policy::DEVICE_MODE_DEMO && !domain.empty() &&
         realm.empty() && !device_id.empty()) ||
        (device_mode == policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH &&
         domain.empty() && realm.empty() && device_id.empty()));
  DCHECK(callback);
  CHECK_EQ(device_lock_running_, false);

  // Check for existing lock first.
  if (device_locked_) {
    if (device_mode != registration_mode_) {
      LOG(ERROR) << "Trying to re-lock with wrong mode: device_mode: "
                 << device_mode
                 << ", registration_mode: " << registration_mode_;
      std::move(callback).Run(LOCK_WRONG_MODE);
      return;
    }

    if (domain != registration_domain_ || realm != registration_realm_ ||
        device_id != registration_device_id_) {
      LOG(ERROR) << "Trying to re-lock with non-matching parameters.";
      std::move(callback).Run(LOCK_WRONG_DOMAIN);
      return;
    }

    // Already locked in the right mode, signal success.
    ReportExistingLockUma(true /* is_existing_lock */);
    std::move(callback).Run(LOCK_SUCCESS);
    return;
  }

  // In case the consistency check is still running, postpone the device locking
  // until it has finished.  This should not introduce additional delay since
  // device locking must wait for TPM initialization anyways.
  if (consistency_check_running_) {
    CHECK(post_check_action_.is_null());
    post_check_action_ = base::BindOnce(
        &InstallAttributes::LockDevice, weak_ptr_factory_.GetWeakPtr(),
        device_mode, domain, realm, device_id, std::move(callback));
    return;
  }

  device_lock_running_ = true;
  cryptohome_client_->InstallAttributesIsReady(
      base::BindOnce(&InstallAttributes::LockDeviceIfAttributesIsReady,
                     weak_ptr_factory_.GetWeakPtr(), device_mode, domain, realm,
                     device_id, std::move(callback)));
}

void InstallAttributes::LockDeviceIfAttributesIsReady(
    policy::DeviceMode device_mode,
    const std::string& domain,
    const std::string& realm,
    const std::string& device_id,
    LockResultCallback callback,
    base::Optional<bool> is_ready) {
  if (!is_ready.has_value() || !is_ready.value()) {
    device_lock_running_ = false;
    std::move(callback).Run(LOCK_NOT_READY);
    return;
  }

  // Clearing the TPM password seems to be always a good deal.
  if (tpm_util::TpmIsEnabled() && !tpm_util::TpmIsBeingOwned() &&
      tpm_util::TpmIsOwned()) {
    cryptohome_client_->CallTpmClearStoredPasswordAndBlock();
  }

  // Make sure we really have a working InstallAttrs.
  if (tpm_util::InstallAttributesIsInvalid()) {
    LOG(ERROR) << "Install attributes invalid.";
    device_lock_running_ = false;
    std::move(callback).Run(LOCK_BACKEND_INVALID);
    return;
  }

  if (!tpm_util::InstallAttributesIsFirstInstall()) {
    LOG(ERROR) << "Install attributes already installed.";
    device_lock_running_ = false;
    std::move(callback).Run(LOCK_ALREADY_LOCKED);
    return;
  }

  // Set values in the InstallAttrs.
  std::string kiosk_enabled, enterprise_owned;
  if (device_mode == policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH) {
    kiosk_enabled = "true";
  } else {
    enterprise_owned = "true";
  }
  std::string mode = GetDeviceModeString(device_mode);
  if (!tpm_util::InstallAttributesSet(kAttrConsumerKioskEnabled,
                                      kiosk_enabled) ||
      !tpm_util::InstallAttributesSet(kAttrEnterpriseOwned, enterprise_owned) ||
      !tpm_util::InstallAttributesSet(kAttrEnterpriseMode, mode) ||
      !tpm_util::InstallAttributesSet(kAttrEnterpriseDomain, domain) ||
      !tpm_util::InstallAttributesSet(kAttrEnterpriseRealm, realm) ||
      !tpm_util::InstallAttributesSet(kAttrEnterpriseDeviceId, device_id)) {
    LOG(ERROR) << "Failed writing attributes.";
    device_lock_running_ = false;
    std::move(callback).Run(LOCK_SET_ERROR);
    return;
  }

  if (!tpm_util::InstallAttributesFinalize() ||
      tpm_util::InstallAttributesIsFirstInstall()) {
    LOG(ERROR) << "Failed locking.";
    device_lock_running_ = false;
    std::move(callback).Run(LOCK_FINALIZE_ERROR);
    return;
  }

  ReadImmutableAttributes(
      base::BindOnce(&InstallAttributes::OnReadImmutableAttributes,
                     weak_ptr_factory_.GetWeakPtr(), device_mode, domain, realm,
                     device_id, std::move(callback)));
}

void InstallAttributes::OnReadImmutableAttributes(policy::DeviceMode mode,
                                                  const std::string& domain,
                                                  const std::string& realm,
                                                  const std::string& device_id,
                                                  LockResultCallback callback) {
  device_lock_running_ = false;

  if (registration_mode_ != mode || registration_domain_ != domain ||
      registration_realm_ != realm || registration_device_id_ != device_id) {
    LOG(ERROR) << "Locked data doesn't match.";
    std::move(callback).Run(LOCK_READBACK_ERROR);
    return;
  }

  ReportExistingLockUma(false /* is_existing_lock */);
  std::move(callback).Run(LOCK_SUCCESS);
}

bool InstallAttributes::IsEnterpriseManaged() const {
  if (!device_locked_) {
    return false;
  }
  return registration_mode_ == policy::DEVICE_MODE_ENTERPRISE ||
         registration_mode_ == policy::DEVICE_MODE_ENTERPRISE_AD ||
         registration_mode_ == policy::DEVICE_MODE_DEMO;
}

bool InstallAttributes::IsActiveDirectoryManaged() const {
  if (!device_locked_) {
    return false;
  }
  return registration_mode_ == policy::DEVICE_MODE_ENTERPRISE_AD;
}

bool InstallAttributes::IsCloudManaged() const {
  if (!device_locked_) {
    return false;
  }
  return registration_mode_ == policy::DEVICE_MODE_ENTERPRISE ||
         registration_mode_ == policy::DEVICE_MODE_DEMO;
}

bool InstallAttributes::IsConsumerKioskDeviceWithAutoLaunch() {
  return device_locked_ &&
         registration_mode_ == policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH;
}

void InstallAttributes::TriggerConsistencyCheck(int dbus_retries) {
  cryptohome_client_->TpmGetPassword(
      base::BindOnce(&InstallAttributes::OnTpmGetPasswordCompleted,
                     weak_ptr_factory_.GetWeakPtr(), dbus_retries));
}

void InstallAttributes::OnTpmGetPasswordCompleted(
    int dbus_retries_remaining,
    base::Optional<std::string> result) {
  if (!result.has_value() && dbus_retries_remaining) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&InstallAttributes::TriggerConsistencyCheck,
                       weak_ptr_factory_.GetWeakPtr(),
                       dbus_retries_remaining - 1),
        base::TimeDelta::FromSeconds(kDbusRetryIntervalInSeconds));
    return;
  }

  base::HistogramBase::Sample state;
  // If the result has a value, we are interested if install attributes file
  // exists (device_locked_), if the device is enrolled (registration_mode_) and
  // if the TPM is locked, meaning the TPM password is deleted so
  // the value from result is empty.
  if (result.has_value()) {
    const bool is_cloud_managed =
        registration_mode_ == policy::DEVICE_MODE_ENTERPRISE ||
        registration_mode_ == policy::DEVICE_MODE_DEMO;
    state = (device_locked_ ? 1 : 0) | (is_cloud_managed ? 2 : 0) |
            (result->empty() ? 4 : 0);
  } else {
    state = 8;
  }
  UMA_HISTOGRAM_ENUMERATION("Enterprise.AttributesTPMConsistency", state, 9);

  // Run any action (LockDevice call) that might have queued behind the
  // consistency check.
  consistency_check_running_ = false;
  if (post_check_action_) {
    std::move(post_check_action_).Run();
    post_check_action_.Reset();
  }
}

// Warning: The values for these keys (but not the keys themselves) are stored
// in the protobuf with a trailing zero.  Also note that some of these constants
// have been copied to login_manager/device_policy_service.cc.  Please make sure
// that all changes to the constants are reflected there as well.
const char InstallAttributes::kConsumerDeviceMode[] = "consumer";
const char InstallAttributes::kEnterpriseDeviceMode[] = "enterprise";
const char InstallAttributes::kEnterpriseADDeviceMode[] = "enterprise_ad";
const char InstallAttributes::kLegacyRetailDeviceMode[] = "kiosk";
const char InstallAttributes::kConsumerKioskDeviceMode[] = "consumer_kiosk";
const char InstallAttributes::kDemoDeviceMode[] = "demo_mode";

const char InstallAttributes::kAttrEnterpriseDeviceId[] =
    "enterprise.device_id";
const char InstallAttributes::kAttrEnterpriseDomain[] = "enterprise.domain";
const char InstallAttributes::kAttrEnterpriseRealm[] = "enterprise.realm";
const char InstallAttributes::kAttrEnterpriseMode[] = "enterprise.mode";
const char InstallAttributes::kAttrEnterpriseOwned[] = "enterprise.owned";
const char InstallAttributes::kAttrEnterpriseUser[] = "enterprise.user";
const char InstallAttributes::kAttrConsumerKioskEnabled[] =
    "consumer.app_kiosk_enabled";

void InstallAttributes::OnCryptohomeServiceInitiallyAvailable(
    bool service_is_ready) {
  if (!service_is_ready)
    LOG(ERROR) << "Failed waiting for cryptohome D-Bus service availability.";

  // Start the consistency check even if we failed to wait for availability;
  // hopefully the service will become available eventually.
  TriggerConsistencyCheck(kDbusRetryCount);
}

std::string InstallAttributes::GetDeviceModeString(policy::DeviceMode mode) {
  switch (mode) {
    case policy::DEVICE_MODE_CONSUMER:
      return InstallAttributes::kConsumerDeviceMode;
    case policy::DEVICE_MODE_ENTERPRISE:
      return InstallAttributes::kEnterpriseDeviceMode;
    case policy::DEVICE_MODE_ENTERPRISE_AD:
      return InstallAttributes::kEnterpriseADDeviceMode;
    case policy::DEVICE_MODE_LEGACY_RETAIL_MODE:
      return InstallAttributes::kLegacyRetailDeviceMode;
    case policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH:
      return InstallAttributes::kConsumerKioskDeviceMode;
    case policy::DEVICE_MODE_DEMO:
      return InstallAttributes::kDemoDeviceMode;
    case policy::DEVICE_MODE_PENDING:
    case policy::DEVICE_MODE_NOT_SET:
      break;
  }
  NOTREACHED() << "Invalid device mode: " << mode;
  return std::string();
}

policy::DeviceMode InstallAttributes::GetDeviceModeFromString(
    const std::string& mode) {
  if (mode == InstallAttributes::kConsumerDeviceMode)
    return policy::DEVICE_MODE_CONSUMER;
  if (mode == InstallAttributes::kEnterpriseDeviceMode)
    return policy::DEVICE_MODE_ENTERPRISE;
  if (mode == InstallAttributes::kEnterpriseADDeviceMode)
    return policy::DEVICE_MODE_ENTERPRISE_AD;
  if (mode == InstallAttributes::kLegacyRetailDeviceMode)
    return policy::DEVICE_MODE_LEGACY_RETAIL_MODE;
  if (mode == InstallAttributes::kConsumerKioskDeviceMode)
    return policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH;
  if (mode == InstallAttributes::kDemoDeviceMode)
    return policy::DEVICE_MODE_DEMO;
  return policy::DEVICE_MODE_NOT_SET;
}

void InstallAttributes::DecodeInstallAttributes(
    const std::map<std::string, std::string>& attr_map) {
  // Start from a clean slate.
  registration_mode_ = policy::DEVICE_MODE_NOT_SET;
  registration_domain_.clear();
  registration_realm_.clear();
  registration_device_id_.clear();

  const std::string enterprise_owned =
      ReadMapKey(attr_map, kAttrEnterpriseOwned);
  const std::string consumer_kiosk_enabled =
      ReadMapKey(attr_map, kAttrConsumerKioskEnabled);
  const std::string mode = ReadMapKey(attr_map, kAttrEnterpriseMode);
  const std::string domain = ReadMapKey(attr_map, kAttrEnterpriseDomain);
  const std::string realm = ReadMapKey(attr_map, kAttrEnterpriseRealm);
  const std::string device_id = ReadMapKey(attr_map, kAttrEnterpriseDeviceId);
  const std::string user_deprecated = ReadMapKey(attr_map, kAttrEnterpriseUser);

  if (enterprise_owned == "true") {
    WarnIfNonempty(attr_map, kAttrConsumerKioskEnabled);
    registration_device_id_ = device_id;

    // Set registration_mode_.
    registration_mode_ = GetDeviceModeFromString(mode);
    if (registration_mode_ != policy::DEVICE_MODE_ENTERPRISE &&
        registration_mode_ != policy::DEVICE_MODE_ENTERPRISE_AD &&
        registration_mode_ != policy::DEVICE_MODE_DEMO) {
      if (!mode.empty()) {
        LOG(WARNING) << "Bad " << kAttrEnterpriseMode << ": " << mode;
      }
      registration_mode_ = policy::DEVICE_MODE_ENTERPRISE;
    }

    if (registration_mode_ == policy::DEVICE_MODE_ENTERPRISE ||
        registration_mode_ == policy::DEVICE_MODE_DEMO) {
      // Either set registration_domain_ ...
      WarnIfNonempty(attr_map, kAttrEnterpriseRealm);
      if (!domain.empty()) {
        // The canonicalization is for compatibility with earlier versions.
        registration_domain_ = gaia::CanonicalizeDomain(domain);
      } else if (!user_deprecated.empty()) {
        // Compatibility for pre M19 code.
        registration_domain_ = gaia::ExtractDomainName(user_deprecated);
      } else {
        LOG(WARNING) << "Couldn't read domain.";
      }
    } else {
      // ... or set registration_realm_.
      WarnIfNonempty(attr_map, kAttrEnterpriseDomain);
      if (!realm.empty()) {
        registration_realm_ = realm;
      } else {
        LOG(WARNING) << "Couldn't read realm.";
      }
    }

    return;
  }

  WarnIfNonempty(attr_map, kAttrEnterpriseOwned);
  WarnIfNonempty(attr_map, kAttrEnterpriseDomain);
  WarnIfNonempty(attr_map, kAttrEnterpriseRealm);
  WarnIfNonempty(attr_map, kAttrEnterpriseDeviceId);
  WarnIfNonempty(attr_map, kAttrEnterpriseUser);
  if (consumer_kiosk_enabled == "true") {
    registration_mode_ = policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH;
    return;
  }

  WarnIfNonempty(attr_map, kAttrConsumerKioskEnabled);
  if (user_deprecated.empty()) {
    registration_mode_ = policy::DEVICE_MODE_CONSUMER;
  }
}

}  // namespace chromeos
