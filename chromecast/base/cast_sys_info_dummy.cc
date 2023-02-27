// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/cast_sys_info_dummy.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"

namespace chromecast {

namespace {
const char kJsonKeyProductName[] = "product_name";
const char kJsonKeySerialNumber[] = "serial_number";
const char kJsonKeyDeviceModel[] = "device_model";
const char kJsonKeyManufacture[] = "manufacturer";

const char kDefaultProductName[] = "cast_shell";
const char kDefaultSerialNumber[] = "dummy.serial.number";
const char kDefaultDeviceModel[] = "dummy model";
const char kDefaultManufacturer[] = "google";

std::string GetStringValue(const base::Value& sys_info_file,
                           const std::string& key,
                           const std::string& default_val) {
  DCHECK(sys_info_file.is_dict());

  const std::string* val = sys_info_file.GetDict().FindString(key);
  if (!val) {
    LOG(WARNING) << "Json key not found: " << key;
    return default_val;
  }
  return *val;
}
}  // namespace

CastSysInfoDummy::CastSysInfoDummy()
    : build_type_(BUILD_ENG),
      serial_number_("dummy.serial.number"),
      product_name_("cast_shell"),
      device_model_("dummy model"),
      board_name_("dummy board"),
      manufacturer_("google"),
      system_build_number_(__DATE__ " - " __TIME__),
      factory_country_("US"),
      factory_locale_list_({"en-US"}) {}

CastSysInfoDummy::CastSysInfoDummy(const std::string& sys_info_file)
    : CastSysInfoDummy() {
  std::string content;
  if (!base::ReadFileToString(base::FilePath(sys_info_file), &content)) {
    LOG(ERROR) << "Failed to read sys info file: " << sys_info_file;
    return;
  }

  auto value = base::JSONReader::Read(content);
  if (!value || !value->is_dict()) {
    LOG(ERROR)
        << "Invaild sys info json file, using the default values instead.";
    return;
  }

  product_name_ =
      GetStringValue(*value, kJsonKeyProductName, kDefaultProductName);
  serial_number_ =
      GetStringValue(*value, kJsonKeySerialNumber, kDefaultSerialNumber);
  device_model_ =
      GetStringValue(*value, kJsonKeyDeviceModel, kDefaultDeviceModel);
  manufacturer_ =
      GetStringValue(*value, kJsonKeyManufacture, kDefaultManufacturer);
}

CastSysInfoDummy::~CastSysInfoDummy() {}

CastSysInfo::BuildType CastSysInfoDummy::GetBuildType() {
  return build_type_;
}

std::string CastSysInfoDummy::GetSystemReleaseChannel() {
  return system_release_channel_;
}

std::string CastSysInfoDummy::GetSerialNumber() {
  return serial_number_;
}

std::string CastSysInfoDummy::GetProductName() {
  return product_name_;
}

std::string CastSysInfoDummy::GetDeviceModel() {
  return device_model_;
}

std::string CastSysInfoDummy::GetBoardName() {
  return board_name_;
}

std::string CastSysInfoDummy::GetBoardRevision() {
  return board_revision_;
}

std::string CastSysInfoDummy::GetManufacturer() {
  return manufacturer_;
}

std::string CastSysInfoDummy::GetSystemBuildNumber() {
  return system_build_number_;
}

std::string CastSysInfoDummy::GetFactoryCountry() {
  return factory_country_;
}

std::vector<std::string> CastSysInfoDummy::GetFactoryLocaleList() {
  return factory_locale_list_;
}

std::string CastSysInfoDummy::GetWifiInterface() {
  return wifi_interface_;
}

std::string CastSysInfoDummy::GetApInterface() {
  return ap_interface_;
}

std::string CastSysInfoDummy::GetProductSsidSuffix() {
  return ssid_suffix_;
}

void CastSysInfoDummy::SetBuildTypeForTesting(
    CastSysInfo::BuildType build_type) {
  build_type_ = build_type;
}

void CastSysInfoDummy::SetSystemReleaseChannelForTesting(
    const std::string& system_release_channel) {
  system_release_channel_ = system_release_channel;
}

void CastSysInfoDummy::SetSerialNumberForTesting(
    const std::string& serial_number) {
  serial_number_ = serial_number;
}

void CastSysInfoDummy::SetProductNameForTesting(
    const std::string& product_name) {
  product_name_ = product_name;
}

void CastSysInfoDummy::SetDeviceModelForTesting(
    const std::string& device_model) {
  device_model_ = device_model;
}

void CastSysInfoDummy::SetBoardNameForTesting(const std::string& board_name) {
  board_name_ = board_name;
}

void CastSysInfoDummy::SetBoardRevisionForTesting(
    const std::string& board_revision) {
  board_revision_ = board_revision;
}

void CastSysInfoDummy::SetManufacturerForTesting(
    const std::string& manufacturer) {
  manufacturer_ = manufacturer;
}

void CastSysInfoDummy::SetSystemBuildNumberForTesting(
    const std::string& system_build_number) {
  system_build_number_ = system_build_number;
}

void CastSysInfoDummy::SetFactoryCountryForTesting(
    const std::string& factory_country) {
  factory_country_ = factory_country;
}

void CastSysInfoDummy::SetFactoryLocaleListForTesting(
    const std::vector<std::string>& factory_locale_list) {
  factory_locale_list_ = factory_locale_list;
}

void CastSysInfoDummy::SetWifiInterfaceForTesting(
    const std::string& wifi_interface) {
  wifi_interface_ = wifi_interface;
}

void CastSysInfoDummy::SetApInterfaceForTesting(
    const std::string& ap_interface) {
  ap_interface_ = ap_interface;
}

void CastSysInfoDummy::SetProductSsidSuffixForTesting(
    const std::string& ssid_suffix) {
  ssid_suffix_ = ssid_suffix;
}

}  // namespace chromecast
