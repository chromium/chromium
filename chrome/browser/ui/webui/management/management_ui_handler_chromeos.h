// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_UI_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_UI_HANDLER_CHROMEOS_H_

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace policy {
class DeviceCloudPolicyManagerAsh;
class StatusCollector;
class SystemLogUploader;
}  // namespace policy

namespace ash {
class SecureDnsManager;
}

class ManagementUIHandlerChromeOS : public BitmapFetcherDelegate,
                                    public ManagementUIHandler {
 public:
  explicit ManagementUIHandlerChromeOS(Profile* profile);
  ManagementUIHandlerChromeOS(const ManagementUIHandlerChromeOS&) = delete;
  ManagementUIHandlerChromeOS& operator=(const ManagementUIHandlerChromeOS&) =
      delete;
  ~ManagementUIHandlerChromeOS() override;

  // ManagementUIHandler
  void RegisterMessages() override;

  // Returns the list of device reporting items for a given profile.
  static base::Value::List GetDeviceReportingInfo(
      const policy::DeviceCloudPolicyManagerAsh* manager,
      Profile* profile);
  static void AddDlpDeviceReportingElementForTesting(
      base::Value::List* report_sources,
      const std::string& message_id);
  static void AddDeviceReportingInfoForTesting(
      base::Value::List* report_sources,
      const policy::StatusCollector* collector,
      const policy::SystemLogUploader* uploader,
      Profile* profile);

  void SetDeviceManagedForTesting(bool managed) { device_managed_ = managed; }

 protected:
  virtual const policy::DeviceCloudPolicyManagerAsh*
  GetDeviceCloudPolicyManager() const;
  // Virtual for testing
  virtual const std::string GetDeviceManager() const;
  virtual bool IsUpdateRequiredEol() const;
  // Adds device return instructions for a managed user as an update is required
  // as per device policy but the device cannot be updated due to End of Life
  // (Auto Update Expiration).
  void AddUpdateRequiredEolInfo(base::Value::Dict* response) const;
  // Adds a boolean which indicates if the network traffic can be monitored by
  // the admin via policy configurations, either via a proxy server, via
  // secure DNS templates with identifiers, or via XDR monitoring. If true, a
  // warning will be added to the transparency panel to inform the user that the
  // admin may be able to see their network traffic.
  void AddMonitoredNetworkPrivacyDisclosure(base::Value::Dict* response);
  // Adds flags indicating whether Desk Sync feature is active for windows
  // and/or cookies. If at least one of them is true, a dedicated section will
  // be added to inform the user that their data is being synced across their
  // ChromeOS devices.
  void AddDeskSyncNotice(Profile* profile, base::Value::Dict* response);

  // ManagementUIHandler
  void RegisterPrefChange(PrefChangeRegistrar& pref_registrar) override;

  std::u16string GetFilesUploadToCloudInfo(Profile* profile);

  virtual const ash::SecureDnsManager* GetSecureDnsManager() const;

  base::Value::Dict GetContextualManagedData(Profile* profile) override;

  // ManagementUIHandler
  bool managed() const override;
  void UpdateManagedState() override;
  bool UpdateDeviceManagedState();

 private:
  void AsyncUpdateLogo();

  // BitmapFetcherDelegate
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  void NotifyPluginVmDataCollectionUpdated();

  void GetManagementStatus(Profile* profile, base::Value::Dict* status) const;

  void HandleGetLocalTrustRootsInfo(const base::Value::List& args);
  void HandleGetFilesUploadToCloudInfo(const base::Value::List& args);
  void HandleGetDeviceReportingInfo(const base::Value::List& args);
  void HandleGetPluginVmDataCollectionStatus(const base::Value::List& args);

  bool device_managed_ = false;

  GURL logo_url_;
  std::string fetched_image_;
  std::unique_ptr<BitmapFetcher> icon_fetcher_;

  base::WeakPtrFactory<ManagementUIHandlerChromeOS> weak_factory_{this};
};
#endif  // CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_UI_HANDLER_CHROMEOS_H_
