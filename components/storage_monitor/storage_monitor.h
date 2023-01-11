// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_H_
#define COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/storage_monitor/storage_info.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "services/device/public/mojom/mtp_manager.mojom-forward.h"
#endif

class MediaFileSystemRegistryTest;
class MediaGalleriesPlatformAppBrowserTest;
class SystemStorageApiTest;
class SystemStorageEjectApiTest;

namespace storage_monitor {

class RemovableStorageObserver;
class TransientDeviceIds;

// Base class for platform-specific instances watching for removable storage
// attachments/detachments.
// Lifecycle contracts: This class is created in the browser process
// before the profile is initialized, so listeners can be
// created during profile construction. The platform-specific initialization,
// which can lead to calling registered listeners with notifications of
// attached volumes, are done lazily at first use through the async
// |EnsureInitialized()| method. That must be done before any of the registered
// listeners will receive updates or calls to other API methods return
// meaningful results.
// A post-initialization |GetAttachedStorage()| call coupled with a
// registered listener will receive a complete set, albeit potentially with
// duplicates. This is because there's no tracking between when listeners were
// registered and the state of initialization, and the fact that platforms
// behave differently in how these notifications are provided.
class StorageMonitor {
 public:
  // This interface is provided to generators of storage notifications.
  class Receiver {
   public:
    virtual ~Receiver();

    virtual void ProcessAttach(const StorageInfo& info) = 0;
    virtual void ProcessDetach(const std::string& id) = 0;
    virtual void MarkInitialized() = 0;
  };

  // Status codes for the result of an EjectDevice() call.
  enum EjectStatus {
    EJECT_OK,
    EJECT_IN_USE,
    EJECT_NO_SUCH_DEVICE,
    EJECT_FAILURE
  };

  // Instantiates the StorageMonitor singleton. This function does not
  // guarantee the complete initialization of the object. For that, see
  // |EnsureInitialized|.
  static void Create();

  // Destroys the StorageMonitor singleton.
  static void Destroy();

  // Returns a pointer to an object owned by BrowserProcess, with lifetime
  // starting before main message loop start, and ending after main message loop
  // shutdown. Called outside it's lifetime (or with no browser process),
  // returns NULL.
  static StorageMonitor* GetInstance();

  static void SetStorageMonitorForTesting(
      std::unique_ptr<StorageMonitor> storage_monitor);

  virtual ~StorageMonitor();

  // Ensures that the storage monitor is initialized. The provided callback, if
  // non-null, will be called when initialization is complete. If initialization
  // has already completed, this callback will be invoked within the calling
  // stack. Before the callback is run, calls to |GetAllAvailableStorages| and
  // |GetStorageInfoForPath| may not return the correct results. In addition,
  // registered observers will not be notified on device attachment/detachment.
  // Callbacks will run on the same sequence as the rest of the class.
  void EnsureInitialized(base::OnceClosure callback);

  // Return true if the storage monitor has already been initialized.
  bool IsInitialized() const;

  // Finds the device that contains |path| and populates |device_info|.
  // Should be able to handle any path on the local system, not just removable
  // storage. Returns false if unable to find the device.
  virtual bool GetStorageInfoForPath(
      const base::FilePath& path,
      StorageInfo* device_info) const = 0;

// TODO(gbillock): make this either unnecessary (implementation-specific) or
// platform-independent.
#if BUILDFLAG(IS_WIN)
  // Gets the MTP device storage information specified by |storage_device_id|.
  // On success, returns true and fills in |device_location| with device
  // interface details and |storage_object_id| with the string ID that
  // uniquely identifies the object on the device. This ID need not be
  // persistent across sessions.
  virtual bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      std::wstring* device_location,
      std::wstring* storage_object_id) const = 0;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  virtual device::mojom::MtpManager* media_transfer_protocol_manager() = 0;
#endif

  // Returns information for all known storages on the system,
  // including fixed and removable storages.
  std::vector<StorageInfo> GetAllAvailableStorages() const;

  void AddObserver(RemovableStorageObserver* obs);
  void RemoveObserver(RemovableStorageObserver* obs);

  std::string GetTransientIdForDeviceId(const std::string& device_id);
  std::string GetDeviceIdForTransientId(const std::string& transient_id) const;

  virtual void EjectDevice(const std::string& device_id,
                           base::OnceCallback<void(EjectStatus)> callback);

 protected:
  friend class ::MediaFileSystemRegistryTest;
  friend class ::MediaGalleriesPlatformAppBrowserTest;
  friend class ::SystemStorageApiTest;
  friend class ::SystemStorageEjectApiTest;

  StorageMonitor();

  virtual Receiver* receiver() const;

  // Called to initialize the storage monitor.
  virtual void Init() = 0;

  // Called by subclasses to mark the storage monitor as fully initialized.
  void MarkInitialized();

 private:
  // Internal method for creating platform specific StorageMonitor.
  static StorageMonitor* CreateInternal();

  class ReceiverImpl;
  friend class ReceiverImpl;

  // Key: device id.
  typedef std::map<std::string, StorageInfo> StorageMap;

  void ProcessAttach(const StorageInfo& storage);
  void ProcessDetach(const std::string& id);

  std::unique_ptr<Receiver> receiver_;

  scoped_refptr<base::ObserverListThreadSafe<RemovableStorageObserver>>
      observer_list_;

  // Used to make sure we call initialize from the same sequence as creation.
  SEQUENCE_CHECKER(sequence_checker_);

  bool initializing_;
  bool initialized_;
  std::vector<base::OnceClosure> on_initialize_callbacks_;

  // For manipulating storage_map_ structure.
  mutable base::Lock storage_lock_;

  // Map of all known storage devices,including fixed and removable storages.
  StorageMap storage_map_;

  std::unique_ptr<TransientDeviceIds> transient_device_ids_;
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_STORAGE_MONITOR_H_
