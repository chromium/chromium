// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_USB_HOST_PERMISSION_MANAGER_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_USB_HOST_PERMISSION_MANAGER_H_

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/usb/usb_host_ui_delegate.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace arc {

class ArcUsbHostBridge;
class ArcUsbHostPermissionTest;

class ArcUsbHostPermissionManager : public ArcAppListPrefs::Observer,
                                    public ArcUsbHostUiDelegate,
                                    public KeyedService {
 public:
  struct UsbDeviceEntry {
    UsbDeviceEntry(const std::string& guid,
                   const base::string16& device_name,
                   const base::string16& serial_number,
                   uint16_t vendor_id,
                   uint16_t product_id);
    UsbDeviceEntry(const UsbDeviceEntry& other);
    // Returns if the device entry is considered as persistent. Granted
    // permission for persistent device will persist when device is
    // removed.
    bool IsPersistent() const { return !serial_number.empty(); }
    // Checks if two device entries matches. If both devices are persistent,
    // check if their serial_number, vendor_id and product_id matches. Otherwise
    // check if therr guid matches.
    bool Matches(const UsbDeviceEntry& other) const;

    // This field can be null if device is persistent and the entry is restored
    // from Chrome prefs. But it can not be null if the entry is not persistent
    // or it is owned by UsbPermissionRequest.
    std::string guid;
    // Device name which is shown in the permission dialog.
    base::string16 device_name;
    // Serial_number of the device. If this field is null if device is
    // considered as non-persistent.
    base::string16 serial_number;
    // Vendor_id of the device.
    uint16_t vendor_id;
    // Product id of the device.
    uint16_t product_id;
  };

  class UsbPermissionRequest {
   public:
    UsbPermissionRequest(
        const std::string& package_name,
        bool is_scan_request,
        base::Optional<UsbDeviceEntry> usb_device_entry,
        base::Optional<ArcUsbHostUiDelegate::RequestPermissionCallback>
            callback);
    UsbPermissionRequest(UsbPermissionRequest&& other);
    UsbPermissionRequest& operator=(UsbPermissionRequest&& other);
    ~UsbPermissionRequest();

    const std::string& package_name() const { return package_name_; }
    bool is_scan_request() const { return is_scan_request_; }
    const base::Optional<UsbDeviceEntry>& usb_device_entry() const {
      return usb_device_entry_;
    }

    // Runs |callback_| with |allowed|.
    void Resolve(bool allowed);

   private:
    // Name of the package that is currently requesting permission.
    std::string package_name_;
    // True if the this is a scan device list request. Otherwise it's a device
    // access request. Open to make it a enum if we have more types.
    bool is_scan_request_;
    // Device entry of targeting device access request. nullopt if this is a
    // scan device list request.
    base::Optional<UsbDeviceEntry> usb_device_entry_;
    // Callback of the device access reqeust. nullopt if this is a scan device
    // list request.
    base::Optional<RequestPermissionCallback> callback_;

    DISALLOW_COPY_AND_ASSIGN(UsbPermissionRequest);
  };

  ~ArcUsbHostPermissionManager() override;
  static ArcUsbHostPermissionManager* GetForBrowserContext(
      content::BrowserContext* context);

  // ArcUsbHostUiDelegate:
  void RequestUsbScanDeviceListPermission(
      const std::string& package_name,
      ArcUsbHostUiDelegate::RequestPermissionCallback callback) override;
  void RequestUsbAccessPermission(
      const std::string& package_name,
      const std::string& guid,
      const base::string16& serial_number,
      const base::string16& manufacturer_string,
      const base::string16& product_string,
      uint16_t vendor_id,
      uint16_t product_id,
      ArcUsbHostUiDelegate::RequestPermissionCallback callback) override;
  bool HasUsbAccessPermission(const std::string& package_name,
                              const std::string& guid,
                              const base::string16& serial_number,
                              uint16_t vendor_id,
                              uint16_t product_id) const override;
  void GrantUsbAccessPermission(const std::string& package_name,
                                const std::string& guid,
                                uint16_t vendor_id,
                                uint16_t product_id) override;
  std::unordered_set<std::string> GetEventPackageList(
      const std::string& guid,
      const base::string16& serial_number,
      uint16_t vendor_id,
      uint16_t product_id) const override;
  void DeviceRemoved(const std::string& guid) override;
  void ClearPermissionRequests() override;

  // ArcAppListPrefs::Observer:
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;

  const std::vector<UsbPermissionRequest>& GetPendingRequestsForTesting() {
    return pending_requests_;
  }

  // Clear |usb_scan_devicelist_permission_packages_| and
  // |usb_access_permission_dict_| for testing. Will not affect Chrome prefs.
  void ClearPermissionForTesting();

 private:
  friend class ArcUsbHostPermissionManagerFactory;
  friend class ArcUsbHostPermissionTest;

  ArcUsbHostPermissionManager(Profile* profile,
                              ArcAppListPrefs* arc_app_list_prefs,
                              ArcUsbHostBridge* arc_usb_host_bridge);

  static ArcUsbHostPermissionManager* Create(content::BrowserContext* context);

  // Restores granted permissions. Called in constructor. Device list scan
  // permission and device access permission for persistent devices will be
  // restored.
  void RestorePermissionFromChromePrefs();

  // Tries to process next permission request as when permission dialog is
  // available.
  void MaybeProcessNextPermissionRequest();

  bool HasUsbScanDeviceListPermission(const std::string& package_name) const;

  bool HasUsbAccessPermission(const std::string& package_name,
                              const UsbDeviceEntry& usb_device_entry) const;

  // Callback for UI permission dialog.
  void OnUsbPermissionReceived(UsbPermissionRequest request, bool allowed);

  void UpdateArcUsbScanDeviceListPermission(const std::string& package_name,
                                            bool allowed);

  void UpdateArcUsbAccessPermission(const std::string& package_name,
                                    const UsbDeviceEntry& usb_device_entry,
                                    bool allowed);

  std::vector<UsbPermissionRequest> pending_requests_;

  // Package permissions will be removed when package is uninstalled.
  // Packages that have been granted permission to scan device list. It will
  // be also stored in Chrome prefs. We may need create UI to revoke this
  // permission.
  std::unordered_set<std::string> usb_scan_devicelist_permission_packages_;

  // Package permissions will be removed when package is uninstalled.
  // Dictory of granted package to devices access permission map.
  // Permissions granted to persistent devices persist when device is removed
  // while permissions granted to ephemeral devices will be removed in such
  // situation.
  std::unordered_multimap<std::string, UsbDeviceEntry>
      usb_access_permission_dict_;

  // Package which made the current USB reuqest.
  std::string current_requesting_package_;

  // Device GUID of targeting device of current USB request. Empty if it's a
  // scan request.
  std::string current_requesting_guid_;

  // True if the permission dialog is currently being shown. Any permission
  // request that occurs while this is true will be queued until after the user
  // has resolved the current request.
  bool is_permission_dialog_visible_ = false;

  Profile* const profile_;

  ArcAppListPrefs* const arc_app_list_prefs_;

  base::WeakPtrFactory<ArcUsbHostPermissionManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcUsbHostPermissionManager);
};

}  // namespace arc

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_USB_HOST_PERMISSION_MANAGER_H_
