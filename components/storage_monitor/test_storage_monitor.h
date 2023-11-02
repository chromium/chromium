// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_TEST_STORAGE_MONITOR_H_
#define COMPONENTS_STORAGE_MONITOR_TEST_STORAGE_MONITOR_H_

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/storage_monitor/storage_monitor.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"
#endif

namespace storage_monitor {

class TestStorageMonitor : public StorageMonitor {
 public:
  TestStorageMonitor();
  ~TestStorageMonitor() override;

  void Init() override;

  void MarkInitialized();

  // Create and initialize a new TestStorageMonitor and install it
  // as the StorageMonitor singleton. If there is a StorageMonitor instance
  // already in place, NULL is returned, otherwise the TestStorageMonitor
  // instance is returned. Use |Destroy| to delete the singleton.
  static TestStorageMonitor* CreateAndInstall();

  // Create and initialize a new TestStorageMonitor and install it
  // as the StorageMonitor singleton. TestStorageMonitor is returned for
  // convenience. Use |Destroy| to delete the singleton.
  static TestStorageMonitor* CreateForBrowserTests();

  // Synchronously initialize the current storage monitor.
  static void SyncInitialize();

  bool GetStorageInfoForPath(const base::FilePath& path,
                             StorageInfo* device_info) const override;

#if BUILDFLAG(IS_WIN)
  bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      std::wstring* device_location,
      std::wstring* storage_object_id) const override;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  device::mojom::MtpManager* media_transfer_protocol_manager() override;
#endif

  Receiver* receiver() const override;

  void EjectDevice(
      const std::string& device_id,
      base::OnceCallback<void(StorageMonitor::EjectStatus)> callback) override;

  const std::string& ejected_device() const { return ejected_device_; }

  void AddRemovablePath(const base::FilePath& path);

  bool init_called() const { return init_called_; }

 private:
  // Whether TestStorageMonitor::Init() has been called for not.
  bool init_called_;

  // The last device to be ejected.
  std::string ejected_device_;

  // Paths considered for testing purposes to be on removable storage.
  std::vector<base::FilePath> removable_paths_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  mojo::Remote<device::mojom::MtpManager> media_transfer_protocol_manager_;
#endif
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_TEST_STORAGE_MONITOR_H_
