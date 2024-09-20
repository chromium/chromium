// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_

#include <optional>
#include <string>

#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_device.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_properties.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_request.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_update.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace network {

class SimpleURLLoader;

}  // namespace network

namespace ash {

// State of the fwupd daemon. Enum defined here:
// https://github.com/fwupd/fwupd/blob/4389f9f913588edae7243a8dbed88ce3788c8bc2/libfwupd/fwupd-enums.h
// Keep in sync with corresponding enum in tools/metrics/histograms/enums.xml.
enum class FwupdStatus {
  kUnknown,
  kIdle,
  kLoading,
  kDecompressing,
  kDeviceRestart,
  kDeviceWrite,
  kDeviceVerify,
  kScheduling,
  kDownloading,
  kDeviceRead,
  kDeviceErase,
  kWaitingForAuth,
  kDeviceBusy,
  kShutdown,
  kWaitingForUser,
  kMaxValue = kWaitingForUser,
};

// Used in histograms. Keep in sync with FirmwareUpdateMethodResult in
// tools/metrics/histograms/metadata/chromeos/enums.xml.
enum class MethodResult {
  kSuccess = 0,
  // DEPRECATED: kInstallFailed = 1,
  kFailedToCreateUpdateDirectory = 2,
  // DEPRECATED: kInvalidDestinationFile = 3,
  kInvalidFile = 4,
  kFailedToDownloadToFile = 5,
  kFailedToCreatePatchFile = 6,
  kEmptyPatchFile = 7,
  kInvalidPatchFileUri = 8,
  kInvalidPatchFile = 9,
  kInstallFailedTimeout = 10,
  kFailedToGetFirmwareFilename = 11,

  // All Install Errors returned by fwupd dbus signal
  // These errors are consistent with
  // /chromeos/ash/components/dbus/fwupd/fwupd_client.h
  //
  // Starting values from 100 to keep the Fwupd Error message contiguous in case
  // more error names are added.
  kInternalError = 100,
  kVersionNewerError = 101,
  kVersionSameError = 102,
  kAlreadyPendingError = 103,
  kAuthFailedError = 104,
  kReadError = 105,
  kWriteError = 106,
  kInvalidFileError = 107,
  kNotFoundError = 108,
  kNothingToDoError = 109,
  kNotSupportedError = 110,
  kSignatureInvalidError = 111,
  kAcPowerRequiredError = 112,
  kPermissionDeniedError = 113,
  kBrokenSystemError = 114,
  kBatteryLevelTooLowError = 115,
  kNeedsUserActionError = 116,
  kAuthExpiredError = 117,
  kUnknownError = 118,
  kMaxValue = kUnknownError,
};

// FirmwareUpdateManager contains all logic that runs the firmware update SWA.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_FWUPD) FirmwareUpdateManager
    : public FwupdClient::Observer,
      public firmware_update::mojom::UpdateProvider,
      public firmware_update::mojom::InstallController,
      public NetworkStateHandlerObserver {
 public:
  enum class Source {
    kUI = 0,
    kStartup = 1,
    kUSBChange = 2,
    kInstallComplete = 3,
    kNetworkChange = 4,
    kMaxValue = kNetworkChange,
  };

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

  void AddDeviceRequestObserver(
      mojo::PendingRemote<firmware_update::mojom::DeviceRequestObserver>
          observer) override;

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

  void OnDeviceRequestResponse(FwupdRequest request) override;

  // When the fwupd DBus client gets a response with updates from fwupd,
  // it calls this function and passes the response.
  void OnUpdateListResponse(const std::string& device_id,
                            FwupdUpdateList* updates) override;
  // TODO(jimmyxgong): Implement this function to send property updates via
  // mojo.
  void OnPropertiesChangedResponse(FwupdProperties* properties) override;

  // Query all updates for all devices.
  void RequestAllUpdates(Source source);

  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState* network) override;

  void BindInterface(
      mojo::PendingReceiver<firmware_update::mojom::UpdateProvider>
          pending_receiver);

  void set_should_show_notification_for_test(bool show_notification) {
    should_show_notification_for_test_ = show_notification;
  }

  void set_refresh_remote_for_testing(bool for_testing) {
    refresh_remote_for_testing_ = for_testing;
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

  typedef base::OnceCallback<void(MethodResult)> MethodCallback;

  // Download and prepare the install file for a specific device.
  void StartInstall(const std::string& device_id,
                    const base::FilePath& filepath,
                    MethodCallback callback);

  // Callback handler after fetching the file.
  void OnGetFile(const std::string& device_id,
                 FirmwareInstallOptions options,
                 MethodCallback callback,
                 base::File file);

  // Query the fwupd DBus client to install an update for a certain device.
  void InstallUpdate(const std::string& device_id,
                     FirmwareInstallOptions options,
                     MethodCallback callback,
                     base::File patch_file);

  // Response from fwupd DBus client InstallUpdate call.
  void OnInstallResponse(MethodCallback callback, FwupdDbusResult result);

  // InstallComplete will be called exactly once with a result when an install
  // attempt succeeds or fails for any reason.
  void InstallComplete(MethodResult result);

  void CreateLocalPatchFile(const base::FilePath& cache_path,
                            const std::string& device_id,
                            const base::FilePath& filepath,
                            MethodCallback callback,
                            bool create_dir_success);

  void MaybeDownloadFileToInternal(const base::FilePath& patch_path,
                                   const std::string& device_id,
                                   const base::FilePath& filepath,
                                   MethodCallback callback,
                                   bool write_file_success);

  void DownloadFileToInternal(const base::FilePath& patch_path,
                              const std::string& device_id,
                              const base::FilePath& filepath,
                              MethodCallback callback);

  void OnUrlDownloadedToFile(
      const std::string& device_id,
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      MethodCallback callback,
      base::FilePath download_path);

  // If refresh remote is allowed and call RefreshRemote otherwise continue with
  // RequestUpdates()
  void MaybeRefreshRemote(bool refresh_allowed);

  using DownloadCompleteCallback =
      base::OnceCallback<void(base::FilePath, base::File)>;

  // Refresh LVFS remote metadata by downloading the required files and calling
  // UpdateMetadata dbus function.
  void RefreshRemote();

  void CreateTempFileAndDownload(base::FilePath local_file,
                                 std::string download_filename,
                                 DownloadCompleteCallback callback,
                                 bool create_dir_success);

  void DownloadLvfsMirrorFile(std::string filename,
                              base::FilePath download_filepath,
                              DownloadCompleteCallback callback,
                              bool write_file_success);

  void GetFileDescriptor(
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      DownloadCompleteCallback callback,
      base::FilePath download_path);

  void OnGetChecksumFile(base::FilePath checksum_filepath,
                         base::File checksum_file);

  void GetFirmwareFilename(std::string file_contents);

  void TriggerDownloadOfFirmwareFile(std::string firmware_filename);

  // Call UpdateMetadata dbus api using `checksum_file_` and `firmware_file`.
  void UpdateMetadata(base::FilePath firmware_filepath,
                      base::File firmware_file);

  void OnUpdateMetadataResponse(FwupdDbusResult result);

  // RefreshRemoteComplete will be called exactly once with a result when an
  // attempt to refresh lvfs remote succeeds or fails for any reason.
  // Then continue requesting devices.
  void RefreshRemoteComplete(MethodResult result);

  // Notifies observers registered with ObservePeripheralUpdates() the current
  // list of devices with pending updates (if any).
  void NotifyUpdateListObservers();

  bool HasPendingUpdates();

  void set_fake_url_for_testing(const std::string& fake_url) {
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

  // Gets /tmp directory path to store downloaded files.
  const base::FilePath GetCacheDirPath();

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

  // The most recent FwupdStatus, used for the purpose of recording metrics.
  FwupdStatus last_fwupd_status_ = FwupdStatus::kUnknown;

  // The most recent DeviceRequest, used for the purpose of recording metrics.
  firmware_update::mojom::DeviceRequestPtr last_device_request_ = nullptr;

  // Timestamp of when the last device request began. Used to calculate a
  // duration for metrics.
  std::optional<base::Time> last_request_started_timestamp_;

  // Used to show the firmware update notification and to determine which
  // metric to fire (Startup/Refresh).
  bool is_first_response_ = true;

  // Whether or not fetching updates in inflight.
  bool is_fetching_updates_ = false;

  // Whether Refresh Remote has been requested and pending successful
  // completion.
  bool is_refresh_pending_ = false;

  // Checksum and firmware paths and File objects are held temporarily during
  // download, and are used for cleanup which must be done on task_runner_.
  base::FilePath checksum_filepath_;
  base::FilePath firmware_filepath_;
  base::File checksum_file_;
  base::File firmware_file_;

  // Used only for testing to force notification to appear.
  bool should_show_notification_for_test_ = false;

  // Used only for testing to trigger RefreshRemote and create file in random
  // directory to avoid flakiness
  bool refresh_remote_for_testing_ = false;

  // Remotes for tracking observers that will be notified of changes to the
  // list of firmware updates.
  mojo::RemoteSet<firmware_update::mojom::UpdateObserver>
      update_list_observers_;

  // Remote for tracking observer that will be notified of incoming
  // DeviceRequests.
  mojo::Remote<firmware_update::mojom::DeviceRequestObserver>
      device_request_observer_;

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
