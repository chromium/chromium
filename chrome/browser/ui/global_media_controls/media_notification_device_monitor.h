// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_MONITOR_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_MONITOR_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/system/system_monitor.h"
#include "build/build_config.h"
#include "media/audio/audio_device_description.h"

class MediaNotificationDeviceProvider;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

// Common interface for a class that detects changes in the audio output devices
// connected to the system and notifies observers.
class MediaNotificationDeviceMonitor {
 public:
  virtual ~MediaNotificationDeviceMonitor();

  // Returns a pointer to the appropriate implementation for the OS.
  static std::unique_ptr<MediaNotificationDeviceMonitor> Create(
      MediaNotificationDeviceProvider* device_provider);

  class DevicesChangedObserver : public base::CheckedObserver {
   public:
    // Called when the MediaNotificationDeviceMonitor detects a change in the
    // connected audio output devices.
    virtual void OnDevicesChanged() = 0;
  };

  void AddDevicesChangedObserver(DevicesChangedObserver* obs);
  void RemoveDevicesChangedObserver(DevicesChangedObserver* obs);

  virtual void StartMonitoring() = 0;
  virtual void StopMonitoring() = 0;

 protected:
  bool is_monitoring_ = false;
  MediaNotificationDeviceMonitor();
  base::ObserverList<DevicesChangedObserver> observers_;
};

#if !((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(USE_UDEV))
// Monitors device changes by observing the SystemMonitor
class SystemMonitorDeviceMonitorImpl
    : public MediaNotificationDeviceMonitor,
      public base::SystemMonitor::DevicesChangedObserver {
 public:
  SystemMonitorDeviceMonitorImpl();
  ~SystemMonitorDeviceMonitorImpl() override;

  void StartMonitoring() override;
  void StopMonitoring() override;

  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override;
};
#else
// Monitors device changes by polling the MediaNotificationDeviceProvider
class PollingDeviceMonitorImpl : public MediaNotificationDeviceMonitor {
 public:
  explicit PollingDeviceMonitorImpl(
      MediaNotificationDeviceProvider* device_provider);
  ~PollingDeviceMonitorImpl() override;

  void StartMonitoring() override;
  void StopMonitoring() override;

  static int get_polling_interval_for_testing();

 private:
  FRIEND_TEST_ALL_PREFIXES(PollingDeviceMonitorImplTest,
                           DeviceChangeNotifiesObserver);

  void PollDeviceProvider();
  void OnDeviceDescriptionsRecieved(
      media::AudioDeviceDescriptions descriptions);
  void NotifyObservers();

  const raw_ptr<MediaNotificationDeviceProvider> device_provider_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::vector<std::string> device_ids_;
  bool is_task_posted_ = false;

  base::WeakPtrFactory<PollingDeviceMonitorImpl> weak_ptr_factory_{this};
};
#endif  // !((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) &&
        // defined(USE_UDEV))

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_DEVICE_MONITOR_H_
