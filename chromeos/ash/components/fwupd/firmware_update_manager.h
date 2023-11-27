// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_

#include <string>

#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_device.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_properties.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_update.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace network {

class SimpleURLLoader;

}  // namespace network

namespace ash {
// FirmwareUpdateManager contains all logic that runs the firmware update SWA.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_FWUPD) FirmwareUpdateManager
    : public FwupdClient::Observer,
      public firmware_update::mojom::UpdateProvider,
      public firmware_update::mojom::InstallController {
 public:
  FirmwareUpdateManager();
  FirmwareUpdateManager(const FirmwareUpdateManager&) = delete;
  FirmwareUpdateManager& operator=(const FirmwareUpdateManager&) = delete;
  ~FirmwareUpdateManager() override;

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called to notify observers, primarily notification controllers, that a
    // critical firmware update is available.
    virtual void OnFirmwareUpdateReceived() = 0;
  };

  // Returns true if the global instance is initialized.
  static bool IsInitialized();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // firmware_update::mojom::UpdateProvider
  void ObservePeripheralUpdates(
      mojo::PendingRemote<firmware_update::mojom::UpdateObserver> observer)
      override;

  void PrepareForUpdate(const std::string& device_id,
                        PrepareForUpdateCallback callback) override;

  void FetchInProgressUpdate(FetchInProgressUpdateCallback callback) override;

  // firmware_update::mojom::InstallController
  void BeginUpdate(const std::string& device_id,
                   const base::FilePath& filepath) override;

  void AddUpdateProgressObserver(
      mojo::PendingRemote<firmware_update::mojom::UpdateProgressObserver>
          observer) override;

  // Gets the global instance pointer.
  static FirmwareUpdateManager* Get();

  // Gets the number of cached updates.
  size_t GetUpdateCount() { return updates_.size(); }

  // FwupdClient::Observer:
  // When the fwupd DBus client gets a response with devices from fwupd,
  // it calls this function and passes the response.
  void OnDeviceListResponse(FwupdDeviceList* devices) override;

  // When the fwupd DBus client gets a response with updates from fwupd,
  // it calls this function and passes the response.
  void OnUpdateListResponse(const std::string& device_id,
                            FwupdUpdateList* updates) override;
  void OnInstallResponse(bool success) override;
  // TODO(jimmyxgong): Implement this function to send property updates via
  // mojo.
  void OnPropertiesChangedResponse(FwupdProperties* properties) override;

  // Query all updates for all devices.
  void RequestAllUpdates();

  // TODO(jimmyxgong): This should override the mojo api interface.
  // Download and prepare the install file for a specific device.
  void StartInstall(const std::string& device_id,
                    const base::FilePath& filepath,
                    base::OnceCallback<void()> callback);

  void BindInterface(
      mojo::PendingReceiver<firmware_update::mojom::UpdateProvider>
          pending_receiver);

  void set_should_show_notification_for_test(bool show_notification) {
    should_show_notification_for_test_ = show_notification;
  }

 protected:
  friend class FirmwareUpdateManagerTest;
  // Temporary auxiliary variables for testing.
  // TODO(swifton): Replace with mock observers.
  int on_device_list_response_count_for_testing_ = 0;
  int on_update_list_response_count_for_testing_ = 0;

 private:
  friend class FirmwareUpdateManagerTest;
  // Query the fwupd DBus client for currently connected devices.
  void RequestDevices();

  // Query the fwupd DBus client for updates for a certain device.
  void RequestUpdates(const std::string& device_id);

  // Callback handler after fetching the file descriptor.
  void OnGetFileDescriptor(const std::string& device_id,
                           FirmwareInstallOptions options,
                           base::OnceCallback<void()> callback,
                           base::ScopedFD file_descriptor);

  // Query the fwupd DBus client to install an update for a certain device.
  void InstallUpdate(const std::string& device_id,
                     FirmwareInstallOptions options,
                     base::OnceCallback<void()> callback,
                     base::File patch_file);

  void CreateLocalPatchFile(const base::FilePath& cache_path,
                            const std::string& device_id,
                            const base::FilePath& filepath,
                            base::OnceCallback<void()> callback);

  void MaybeDownloadFileToInternal(const base::FilePath& patch_path,
                                   const std::string& device_id,
                                   const base::FilePath& filepath,
                                   base::OnceCallback<void()> callback,
                                   bool write_file_success);

  void DownloadFileToInternal(const base::FilePath& patch_path,
                              const std::string& device_id,
                              const base::FilePath& filepath,
                              base::OnceCallback<void()> callback);

  void OnUrlDownloadedToFile(
      const std::string& device_id,
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      base::OnceCallback<void()> callback,
      base::FilePath download_path);

  // Notifies observers registered with ObservePeripheralUpdates() the current
  // list of devices with pending updates (if any).
  void NotifyUpdateListObservers();

  bool HasPendingUpdates();

  void SetFakeUrlForTesting(const std::string& fake_url) {
    fake_url_for_testing_ = fake_url;
  }

  // Resets the mojo::Receiver |install_controller_receiver_|
  // and |update_progress_observer_|.
  void ResetInstallState();

  // Checks if any update in |updates_| is critical. If so,
  // a single notification is shown to the user.
  void ShowNotificationIfRequired();

  // Call to notify observers that a new notification is needed.
  void NotifyCriticalFirmwareUpdateReceived();

  // Records the # of devices found at startup and whenever the device list
  // is refreshed.
  void RecordDeviceMetrics(int num_devices);

  // Records the # of updates found at startup and whenever the update list
  // is refreshed.
  void RecordUpdateMetrics();

  int GetNumCriticalUpdates();

  // Map of a device ID to `FwupdDevice` which is waiting for the list
  // of updates.
  base::flat_map<std::string, FwupdDevice> devices_pending_update_;

  // Set of device IDs with critical updates that we've already shown a
  // notification for.
  base::flat_set<std::string> devices_already_notified_;

  // List of all available updates. If `devices_pending_update_` is not
  // empty then this list is not yet complete.
  std::vector<firmware_update::mojom::FirmwareUpdatePtr> updates_;

  // Only used for testing if StartInstall() queries to a fake URL.
  std::string fake_url_for_testing_;

  // The device update that is currently inflight.
  firmware_update::mojom::FirmwareUpdatePtr inflight_update_;

  // Used to show the firmware update notification and to determine which
  // metric to fire (Startup/Refresh).
  bool is_first_response_ = true;

  // Whether or not fetching updates in inflight.
  bool is_fetching_updates_ = false;

  // Used only for testing to force notification to appear.
  bool should_show_notification_for_test_ = false;

  // Remotes for tracking observers that will be notified of changes to the
  // list of firmware updates.
  mojo::RemoteSet<firmware_update::mojom::UpdateObserver>
      update_list_observers_;

  // Remote for tracking observer that will be notified of changes to
  // the in-progress update.
  mojo::Remote<firmware_update::mojom::UpdateProgressObserver>
      update_progress_observer_;

  base::ObserverList<Observer> observer_list_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  mojo::Receiver<firmware_update::mojom::UpdateProvider> receiver_{this};

  mojo::Receiver<firmware_update::mojom::InstallController>
      install_controller_receiver_{this};

  base::WeakPtrFactory<FirmwareUpdateManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_
