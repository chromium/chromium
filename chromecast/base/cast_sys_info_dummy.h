// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CAST_SYS_INFO_DUMMY_H_
#define CHROMECAST_BASE_CAST_SYS_INFO_DUMMY_H_

#include <vector>

// Note(slan): This file is needed by internal targets which cannot depend on
// "//base". Amend this include with a comment so gn check ignores it.
#include "chromecast/public/cast_sys_info.h"

namespace chromecast {

// Dummy implementation of CastSysInfo. Fields can be overwritten for test.
class CastSysInfoDummy : public CastSysInfo {
 public:
  CastSysInfoDummy();
  CastSysInfoDummy(const std::string& sys_info_file);

  CastSysInfoDummy(const CastSysInfoDummy&) = delete;
  CastSysInfoDummy& operator=(const CastSysInfoDummy&) = delete;

  ~CastSysInfoDummy() override;

  // CastSysInfo implementation:
  BuildType GetBuildType() override;
  std::string GetSystemReleaseChannel() override;
  std::string GetSerialNumber() override;
  std::string GetProductName() override;
  std::string GetDeviceModel() override;
  std::string GetBoardName() override;
  std::string GetBoardRevision() override;
  std::string GetManufacturer() override;
  std::string GetSystemBuildNumber() override;
  std::string GetFactoryCountry() override;
  std::vector<std::string> GetFactoryLocaleList() override;
  std::string GetWifiInterface() override;
  std::string GetApInterface() override;
  std::string GetProductSsidSuffix() override;

  void SetBuildTypeForTesting(BuildType build_type);
  void SetSystemReleaseChannelForTesting(
      const std::string& system_release_channel);
  void SetSerialNumberForTesting(const std::string& serial_number);
  void SetProductNameForTesting(const std::string& product_name);
  void SetDeviceModelForTesting(const std::string& device_model);
  void SetBoardNameForTesting(const std::string& board_name);
  void SetBoardRevisionForTesting(const std::string& board_revision);
  void SetManufacturerForTesting(const std::string& manufacturer);
  void SetSystemBuildNumberForTesting(const std::string& system_build_number);
  void SetFactoryCountryForTesting(const std::string& factory_country);
  void SetFactoryLocaleListForTesting(
      const std::vector<std::string>& factory_locale_list);
  void SetWifiInterfaceForTesting(const std::string& wifi_interface);
  void SetApInterfaceForTesting(const std::string& ap_interface);
  void SetProductSsidSuffixForTesting(const std::string& ssid_suffix);

 private:
  BuildType build_type_;
  std::string system_release_channel_;
  std::string serial_number_;
  std::string product_name_;
  std::string device_model_;
  std::string board_name_;
  std::string board_revision_;
  std::string manufacturer_;
  std::string system_build_number_;
  std::string factory_country_;
  std::vector<std::string> factory_locale_list_;
  std::string wifi_interface_;
  std::string ap_interface_;
  std::string ssid_suffix_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_CAST_SYS_INFO_DUMMY_H_
