// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_H_
#define COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#if BUILDFLAG(IS_ANDROID)
#include "components/sync/protocol/theme_android_specifics.pb.h"
#else
#include "components/sync/protocol/theme_specifics.pb.h"
#endif
#include "components/sync/protocol/theme_types.pb.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/themes/cross_device/theme_comparer.h"

namespace themes {

#if BUILDFLAG(IS_ANDROID)
using LocalThemeSpecifics = sync_pb::ThemeAndroidSpecifics;
#else
using LocalThemeSpecifics = sync_pb::ThemeSpecifics;
#endif  // BUILDFLAG(IS_ANDROID)

enum class ServiceStatus {
  kInitializing,
  kActive,
  kSyncDisabled,
};

inline syncer::DataType OsTypeToDataType(syncer::DeviceInfo::OsType os_type) {
  switch (os_type) {
    case syncer::DeviceInfo::OsType::kAndroid:
      return syncer::THEMES_ANDROID;
    case syncer::DeviceInfo::OsType::kIOS:
      return syncer::THEMES_IOS;
    case syncer::DeviceInfo::OsType::kWindows:
    case syncer::DeviceInfo::OsType::kMac:
    case syncer::DeviceInfo::OsType::kLinux:
    case syncer::DeviceInfo::OsType::kChromeOsAsh:
      return syncer::THEMES;
    default:
      return syncer::UNSPECIFIED;
  }
}

// Holds theme information from a specific platform and device.
template <typename LocalSpecifics>
struct DeviceThemeInfo {
  DeviceThemeInfo() = default;
  DeviceThemeInfo(const DeviceThemeInfo&) = default;
  DeviceThemeInfo& operator=(const DeviceThemeInfo&) = default;
  ~DeviceThemeInfo() = default;

  bool operator==(const DeviceThemeInfo& other) const {
    return device_name == other.device_name && os_type == other.os_type &&
           form_factor == other.form_factor &&
           ThemeComparer<LocalSpecifics>::Equals(theme, other.theme);
  }

  std::string device_name;
  syncer::DeviceInfo::OsType os_type = syncer::DeviceInfo::OsType::kUnknown;
  syncer::DeviceInfo::FormFactor form_factor =
      syncer::DeviceInfo::FormFactor::kUnknown;
  LocalSpecifics theme;
};

// Base class for tracking theme configurations across devices.
// It maintains a cache of themes from other devices and notifies observers when
// they change. It also manages the sync bridges for the tracked data types.
template <typename LocalSpecifics>
class CrossDeviceThemeTracker : public KeyedService,
                                public syncer::DeviceInfoTracker::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCrossDeviceThemeChanged() = 0;
    virtual void OnServiceStatusChanged(ServiceStatus status) = 0;
  };

  explicit CrossDeviceThemeTracker(
      syncer::DeviceInfoTracker* device_info_tracker)
      : device_info_tracker_(device_info_tracker) {
    if (device_info_tracker_) {
      device_info_tracker_->AddObserver(this);
    }
  }

  CrossDeviceThemeTracker(const CrossDeviceThemeTracker&) = delete;
  CrossDeviceThemeTracker& operator=(const CrossDeviceThemeTracker&) = delete;

  ~CrossDeviceThemeTracker() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (device_info_tracker_) {
      device_info_tracker_->RemoveObserver(this);
    }
  }

  void AddObserver(Observer* observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observers_.RemoveObserver(observer);
  }

  std::vector<DeviceThemeInfo<LocalSpecifics>> GetOtherDevicesThemes() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::vector<DeviceThemeInfo<LocalSpecifics>> themes;
    for (const auto& [_, theme_info] : other_themes_) {
      themes.push_back(theme_info);
    }
    return themes;
  }

  ServiceStatus GetServiceStatus() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return status_;
  }

  void RegisterBridge(syncer::DataType type,
                      std::unique_ptr<syncer::DataTypeSyncBridge> bridge) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(bridge);
    DCHECK(bridges_.find(type) == bridges_.end());
    bridges_[type] = std::move(bridge);
    bridge_statuses_[type] = ServiceStatus::kInitializing;
    UpdateAggregateStatus();
  }

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetSyncDelegateForType(
      syncer::DataType type) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto it = bridges_.find(type);
    if (it == bridges_.end()) {
      return nullptr;
    }
    return it->second->change_processor()->GetControllerDelegate();
  }

  void UpdateThemeInfo(const std::string& cache_guid,
                       DeviceThemeInfo<LocalSpecifics> theme_info) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ResolveDeviceInfo(cache_guid, theme_info);
    auto it = other_themes_.find(cache_guid);
    if (it != other_themes_.end() && it->second == theme_info) {
      return;
    }
    other_themes_[cache_guid] = std::move(theme_info);
    NotifyObservers();
  }

  void RemoveThemeInfo(const std::string& cache_guid) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (other_themes_.erase(cache_guid) > 0) {
      NotifyObservers();
    }
  }

  void OnBridgeSyncStarted(syncer::DataType type) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bridge_statuses_[type] = ServiceStatus::kActive;
    UpdateAggregateStatus();
  }

  void OnBridgeSyncDisabled(syncer::DataType type) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bridge_statuses_[type] = ServiceStatus::kSyncDisabled;
    ClearThemesForDataType(type);
    UpdateAggregateStatus();
  }

  void ClearThemesForDataType(syncer::DataType type) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bool changed = false;
    for (auto it = other_themes_.begin(); it != other_themes_.end();) {
      if (OsTypeToDataType(it->second.os_type) == type) {
        it = other_themes_.erase(it);
        changed = true;
      } else {
        ++it;
      }
    }
    if (changed) {
      NotifyObservers();
    }
  }

  // KeyedService:
  void Shutdown() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (device_info_tracker_) {
      device_info_tracker_->RemoveObserver(this);
      device_info_tracker_ = nullptr;
    }
    observers_.Clear();
    bridges_.clear();
  }

  // DeviceInfoTracker::Observer:
  void OnDeviceInfoChange() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bool changed = false;
    for (auto& [cache_guid, theme_info] : other_themes_) {
      DeviceThemeInfo<LocalSpecifics> updated_info = theme_info;
      ResolveDeviceInfo(cache_guid, updated_info);
      if (theme_info.device_name != updated_info.device_name ||
          theme_info.form_factor != updated_info.form_factor ||
          theme_info.os_type != updated_info.os_type) {
        theme_info.device_name = updated_info.device_name;
        theme_info.form_factor = updated_info.form_factor;
        theme_info.os_type = updated_info.os_type;
        changed = true;
      }
    }
    if (changed) {
      NotifyObservers();
    }
  }

 protected:
  syncer::DeviceInfoTracker* device_info_tracker() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return device_info_tracker_;
  }

  void NotifyObservers() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (auto& observer : observers_) {
      observer.OnCrossDeviceThemeChanged();
    }
  }

  void UpdateAggregateStatus() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (bridge_statuses_.empty()) {
      SetStatus(ServiceStatus::kInitializing);
      return;
    }

    bool any_initializing = false;
    bool all_disabled = true;

    for (const auto& [_, status] : bridge_statuses_) {
      if (status == ServiceStatus::kInitializing) {
        any_initializing = true;
      }
      if (status != ServiceStatus::kSyncDisabled) {
        all_disabled = false;
      }
    }

    if (any_initializing) {
      SetStatus(ServiceStatus::kInitializing);
    } else if (all_disabled) {
      SetStatus(ServiceStatus::kSyncDisabled);
    } else {
      SetStatus(ServiceStatus::kActive);
    }
  }

  void SetStatus(ServiceStatus status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (status_ == status) {
      return;
    }
    status_ = status;
    for (auto& observer : observers_) {
      observer.OnServiceStatusChanged(status_);
    }
  }

  std::map<syncer::DataType, ServiceStatus> bridge_statuses_;

 private:
  void ResolveDeviceInfo(const std::string& client_tag_hash_value,
                         DeviceThemeInfo<LocalSpecifics>& theme_info) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!device_info_tracker_) {
      return;
    }
    syncer::DataType type = OsTypeToDataType(theme_info.os_type);
    if (type == syncer::UNSPECIFIED) {
      return;
    }
    for (const auto* device : device_info_tracker_->GetAllDeviceInfo()) {
      auto hash = syncer::ClientTagHash::FromUnhashed(type, device->guid());
      if (hash.value() == client_tag_hash_value) {
        theme_info.device_name = device->client_name();
        theme_info.form_factor = device->form_factor();
        theme_info.os_type = device->os_type();
        return;
      }
    }
  }

  raw_ptr<syncer::DeviceInfoTracker> device_info_tracker_;
  base::ObserverList<Observer> observers_;
  std::map<std::string, DeviceThemeInfo<LocalSpecifics>> other_themes_;
  std::map<syncer::DataType, std::unique_ptr<syncer::DataTypeSyncBridge>>
      bridges_;
  ServiceStatus status_ = ServiceStatus::kInitializing;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace themes

#endif  // COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_H_
