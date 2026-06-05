// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_H_
#define COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/protocol/theme_android_specifics.pb.h"
#include "components/sync/protocol/theme_ios_specifics.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync_device_info/device_info.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace syncer {
class DeviceInfoTracker;
}

namespace themes {

enum class ServiceStatus {
  kInitializing,
  kActive,
  kSyncDisabled,
};

// Holds theme information from a specific platform and device.
// This is a unified representation of themes across different platforms
// (Desktop, Android, iOS) and is used to display theme info from other devices
// in the NTP.
struct PlatformThemeInfo {
  PlatformThemeInfo();
  PlatformThemeInfo(const PlatformThemeInfo&);
  PlatformThemeInfo& operator=(const PlatformThemeInfo&);
  ~PlatformThemeInfo();

  bool operator==(const PlatformThemeInfo&) const;

  std::string device_name;
  syncer::DeviceInfo::OsType os_type;
  syncer::DeviceInfo::FormFactor form_factor;

  std::optional<SkColor> color;
  std::optional<sync_pb::UserColorTheme::BrowserColorVariant> color_variant;

  struct Background {
    Background();
    Background(const Background&);
    Background& operator=(const Background&);
    ~Background();

    bool operator==(const Background&) const = default;

    GURL url;
    std::string attribution_line_1;
    std::string attribution_line_2;
  };
  std::optional<Background> background;

  struct Extension {
    Extension();
    Extension(const Extension&);
    Extension& operator=(const Extension&);
    ~Extension();

    bool operator==(const Extension&) const = default;

    std::string id;
    std::string name;
  };
  std::optional<Extension> extension;
};

// Base class for tracking theme configurations across devices.
// It maintains a cache of themes from other devices and notifies observers when
// they change. Derived classes implement platform-specific sync logic.
template <typename LocalSpecifics>
class CrossDeviceThemeTracker : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCrossDeviceThemeChanged() = 0;
    virtual void OnServiceStatusChanged(ServiceStatus status) = 0;
  };

  explicit CrossDeviceThemeTracker(
      syncer::DeviceInfoTracker* device_info_tracker)
      : device_info_tracker_(device_info_tracker) {}

  CrossDeviceThemeTracker(const CrossDeviceThemeTracker&) = delete;
  CrossDeviceThemeTracker& operator=(const CrossDeviceThemeTracker&) = delete;

  ~CrossDeviceThemeTracker() override = default;

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  std::vector<PlatformThemeInfo> GetOtherDevicesThemes() const {
    std::vector<PlatformThemeInfo> themes;
    for (const auto& [_, theme_info] : other_themes_) {
      themes.push_back(theme_info);
    }
    return themes;
  }

  ServiceStatus GetServiceStatus() const { return status_; }

  // KeyedService:
  void Shutdown() override { observers_.Clear(); }

 protected:
  void UpdateThemeInfo(const std::string& cache_guid,
                       PlatformThemeInfo theme_info) {
    auto it = other_themes_.find(cache_guid);
    if (it != other_themes_.end() && it->second == theme_info) {
      return;
    }
    other_themes_[cache_guid] = std::move(theme_info);
    NotifyObservers();
  }

  void RemoveThemeInfo(const std::string& cache_guid) {
    if (other_themes_.erase(cache_guid) > 0) {
      NotifyObservers();
    }
  }

  void SetStatus(ServiceStatus status) {
    if (status_ == status) {
      return;
    }
    status_ = status;
    for (auto& observer : observers_) {
      observer.OnServiceStatusChanged(status_);
    }
  }

  syncer::DeviceInfoTracker* device_info_tracker() {
    return device_info_tracker_;
  }

  void NotifyObservers() {
    for (auto& observer : observers_) {
      observer.OnCrossDeviceThemeChanged();
    }
  }

 private:
  const raw_ptr<syncer::DeviceInfoTracker> device_info_tracker_;
  base::ObserverList<Observer> observers_;
  std::map<std::string, PlatformThemeInfo> other_themes_;
  ServiceStatus status_ = ServiceStatus::kInitializing;
};

}  // namespace themes

#endif  // COMPONENTS_THEMES_CROSS_DEVICE_CROSS_DEVICE_THEME_TRACKER_H_
