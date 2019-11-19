// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_MTP_MANAGER_CLIENT_CHROMEOS_H_
#define COMPONENTS_STORAGE_MONITOR_MTP_MANAGER_CLIENT_CHROMEOS_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/storage_monitor/storage_monitor.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"

namespace base {
class FilePath;
}

namespace storage_monitor {

// This client listens for MTP storage attachment and detachment events
// from MtpManager and forwards them to StorageMonitor.
class MtpManagerClientChromeOS : public device::mojom::MtpManagerClient {
 public:
  MtpManagerClientChromeOS(StorageMonitor::Receiver* receiver,
                           device::mojom::MtpManager* mtp_manager);
  ~MtpManagerClientChromeOS() override;

  // Finds the storage that contains |path| and populates |storage_info|.
  // Returns false if unable to find the storage.
  bool GetStorageInfoForPath(const base::FilePath& path,
                             StorageInfo* storage_info) const;

  void EjectDevice(const std::string& device_id,
                   base::Callback<void(StorageMonitor::EjectStatus)> callback);

 protected:
  // device::mojom::MtpManagerClient implementation.
  // Exposed for unit tests.
  void StorageAttached(device::mojom::MtpStorageInfoPtr storage_info) override;
  void StorageDetached(const std::string& storage_name) override;

 private:
  // Mapping of storage location and MTP storage info object.
  using StorageLocationToInfoMap = std::map<std::string, StorageInfo>;

  // Enumerate existing MTP storage devices.
  void OnReceivedStorages(
      std::vector<device::mojom::MtpStorageInfoPtr> storage_info_list);

  // Find the |storage_map_| key for the record with this |device_id|. Returns
  // true on success, false on failure.
  bool GetLocationForDeviceId(const std::string& device_id,
                              std::string* location) const;

  // Map of all attached MTP devices.
  StorageLocationToInfoMap storage_map_;

  // Pointer to the MTP manager. Not owned. Client must ensure the MTP
  // manager outlives this object.
  device::mojom::MtpManager* const mtp_manager_;

  mojo::AssociatedReceiver<device::mojom::MtpManagerClient> receiver_{this};

  // The notifications object to use to signal newly attached devices.
  // Guaranteed to outlive this class.
  StorageMonitor::Receiver* const notifications_;

  base::WeakPtrFactory<MtpManagerClientChromeOS> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MtpManagerClientChromeOS);
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_MTP_MANAGER_CLIENT_CHROMEOS_H_
