// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/power_metrics/energy_metrics_provider_win.h"

#include <initguid.h>
#include <windows.h>

#include <devioctl.h>
#include <emi.h>
#include <setupapi.h>

#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ptr_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_devinfo.h"
#include "base/win/scoped_handle.h"

namespace power_metrics {

namespace {

// Windows EMI interface provides energy data in units of picowatt-hour.
constexpr double kNanojoulesPerPicowattHour = 3.6;

// These metrics are hardware dependent. "RAPL" metrics are from Intel
// Running Average Power Limit (RAPL) interface, and the rest are from AMD.
// Here, we only consider single-socket system, where "Package0" means that the
// metered hardware are in the same package 0, "PP0" usually stands for cores,
// and "PP1" usually stands for integrated GPU. There should also be "Package1"
// or more packages for more than one socket system, which needs more tests to
// find out.
void SetEnergyMetric(const std::wstring& metric_type,
                     EnergyMetricsProvider::EnergyMetrics& energy_metrics,
                     uint64_t absolute_energy) {
  if (metric_type == L"RAPL_Package0_PKG") {
    energy_metrics.package_nanojoules = absolute_energy;
  } else if (metric_type == L"RAPL_Package0_PP0") {
    energy_metrics.cpu_nanojoules = absolute_energy;
  } else if (metric_type == L"RAPL_Package0_PP1") {
    energy_metrics.gpu_nanojoules = absolute_energy;
  } else if (metric_type == L"RAPL_Package0_DRAM") {
    energy_metrics.dram_nanojoules = absolute_energy;
  } else if (metric_type == L"VDDCR_VDD Energy") {
    energy_metrics.vdd_nanojoules = absolute_energy;
  } else if (metric_type == L"VDDCR_SOC Energy") {
    energy_metrics.soc_nanojoules = absolute_energy;
  } else if (metric_type == L"Current Socket Energy") {
    energy_metrics.socket_nanojoules = absolute_energy;
  } else if (metric_type == L"Apu Energy") {
    energy_metrics.apu_nanojoules = absolute_energy;
  }
}

}  // namespace

EnergyMetricsProviderWin::EnergyMetricsProviderWin() = default;
EnergyMetricsProviderWin::~EnergyMetricsProviderWin() = default;

// static
std::unique_ptr<EnergyMetricsProviderWin> EnergyMetricsProviderWin::Create() {
  return base::WrapUnique(new EnergyMetricsProviderWin());
}

std::optional<EnergyMetricsProvider::EnergyMetrics>
EnergyMetricsProviderWin::CaptureMetrics() {
  if (!Initialize()) {
    handle_.Close();
    return std::nullopt;
  }

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  DWORD bytes_returned = 0;
  std::vector<EMI_CHANNEL_MEASUREMENT_DATA> measurement_data(
      metric_types_.size());
  const size_t measurement_data_size_bytes =
      sizeof(EMI_CHANNEL_MEASUREMENT_DATA) * metric_types_.size();
  // Get the EMI measurement data.
  if (!DeviceIoControl(handle_.get(), IOCTL_EMI_GET_MEASUREMENT, nullptr, 0,
                       measurement_data.data(), measurement_data_size_bytes,
                       &bytes_returned, nullptr)) {
    PLOG(ERROR) << "IOCTL_EMI_GET_MEASUREMENT failed";
    return std::nullopt;
  }
  CHECK_EQ(bytes_returned, measurement_data_size_bytes);

  EnergyMetrics energy_metrics = {0};
  for (size_t i = 0; i < metric_types_.size(); ++i) {
    EMI_CHANNEL_MEASUREMENT_DATA* channel_data = &measurement_data[i];
    uint64_t absolute_energy = static_cast<uint64_t>(
        kNanojoulesPerPicowattHour * channel_data->AbsoluteEnergy);
    SetEnergyMetric(metric_types_[i], energy_metrics, absolute_energy);
  }
  return energy_metrics;
}

bool EnergyMetricsProviderWin::Initialize() {
  if (is_initialized_) {
    if (metric_types_.empty()) {
      return false;
    }
    return true;
  }

  is_initialized_ = true;
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Energy Meter Interface
  // {45BD8344-7ED6-49cf-A440-C276C933B053}
  // https://learn.microsoft.com/en-us/windows-hardware/drivers/powermeter/energy-meter-interface
  //
  // Get device information set for the Energy Meter Interface.
  base::win::ScopedDevInfo dev_info(
      SetupDiGetClassDevs(&GUID_DEVICE_ENERGY_METER, nullptr, nullptr,
                          DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
  if (!dev_info.is_valid()) {
    PLOG(WARNING) << "SetupDiGetClassDevs";
    return false;
  }

  // Pick the first device interface in the returned device information set.
  //
  // TODO(crbug.com/40879127): Determine if the first device interface is always
  // the desired one.
  SP_DEVICE_INTERFACE_DATA dev_data = {0};
  dev_data.cbSize = sizeof(dev_data);
  if (!SetupDiEnumDeviceInterfaces(dev_info.get(), nullptr,
                                   &GUID_DEVICE_ENERGY_METER, 0, &dev_data)) {
    PLOG(WARNING) << "SetupDiEnumDeviceInterfaces";
    return false;
  }

  // Get the required size of device interface detail data.
  DWORD required_size = 0;
  if (SetupDiGetDeviceInterfaceDetail(dev_info.get(), &dev_data, nullptr, 0,
                                      &required_size, nullptr) ||
      ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return false;
  }

  // Get the pointer to an SP_DEVICE_INTERFACE_DETAIL_DATA structure to
  // receive information.
  std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA, base::FreeDeleter>
      dev_detail_data(
          static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(malloc(required_size)));
  dev_detail_data->cbSize = sizeof(*dev_detail_data);
  if (!SetupDiGetDeviceInterfaceDetail(dev_info.get(), &dev_data,
                                       dev_detail_data.get(), required_size,
                                       nullptr, nullptr)) {
    PLOG(WARNING) << "SetupDiGetDeviceInterfaceDetail";
    return false;
  }

  // Get the handle to access Energy Meter Interface.
  handle_.Set(::CreateFile(dev_detail_data->DevicePath, GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!handle_.is_valid()) {
    LOG(WARNING) << "Failed to set up handle";
    return false;
  }

  DWORD bytes_returned = 0;

  // Verify the EMI interface version.
  EMI_VERSION emi_version = {0};
  if (!DeviceIoControl(handle_.get(), IOCTL_EMI_GET_VERSION, nullptr, 0,
                       &emi_version, sizeof(emi_version), &bytes_returned,
                       nullptr)) {
    PLOG(WARNING) << "EMI interface not available";
    return false;
  }
  CHECK_EQ(bytes_returned, sizeof(emi_version));

  if (emi_version.EmiVersion != EMI_VERSION_V2 &&
      emi_version.EmiVersion != EMI_VERSION_V1) {
    LOG(WARNING) << "EMI version not supported, EMI version = "
                 << emi_version.EmiVersion;
    return false;
  }

  // Get the size of the EMI metadata.
  EMI_METADATA_SIZE metadata_size = {0};
  if (!DeviceIoControl(handle_.get(), IOCTL_EMI_GET_METADATA_SIZE, nullptr, 0,
                       &metadata_size, sizeof(metadata_size), &bytes_returned,
                       nullptr)) {
    PLOG(ERROR) << "IOCTL_EMI_GET_METADATA_SIZE";
    return false;
  }
  CHECK_EQ(bytes_returned, sizeof(metadata_size));

  if (!metadata_size.MetadataSize) {
    LOG(ERROR) << "MetadataSize == 0";
    return false;
  }

  // Get the EMI metadata.
  std::vector<char> metadata_buf(metadata_size.MetadataSize);
  if (!DeviceIoControl(handle_.get(), IOCTL_EMI_GET_METADATA, nullptr, 0,
                       metadata_buf.data(), metadata_size.MetadataSize,
                       &bytes_returned, nullptr)) {
    PLOG(ERROR) << "IOCTL_EMI_GET_METADATA";
    return false;
  }
  CHECK_EQ(static_cast<DWORD>(bytes_returned), metadata_buf.size());

  // For different EMI versions, get the types of available metrics
  // respectively.
  if (emi_version.EmiVersion == EMI_VERSION_V1) {
    EMI_METADATA_V1* metadata_v1 =
        reinterpret_cast<EMI_METADATA_V1*>(metadata_buf.data());
    metric_types_.push_back(metadata_v1->MeteredHardwareName);
  } else if (emi_version.EmiVersion == EMI_VERSION_V2) {
    EMI_METADATA_V2* metadata_v2 =
        reinterpret_cast<EMI_METADATA_V2*>(metadata_buf.data());
    EMI_CHANNEL_V2* channel = &metadata_v2->Channels[0];
    // EMI v2 has a different channel for each metric.
    for (int i = 0; i < metadata_v2->ChannelCount; ++i) {
      metric_types_.push_back(channel->ChannelName);
      channel = EMI_CHANNEL_V2_NEXT_CHANNEL(channel);
    }
  }

  if (metric_types_.empty()) {
    LOG(WARNING) << "No available energy metric";
    return false;
  }
  return true;
}

}  // namespace power_metrics
