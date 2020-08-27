// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/probe_service_converters.h"

#include <cstdint>
#include <vector>

#include "chromeos/components/telemetry_extension_ui/convert_ptr.h"
#include "chromeos/components/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace chromeos {
namespace converters {

// Note: in some tests we intentionally use New() with no arguments for
// cros_healthd::mojom types, because there can be some fields that we don't
// test yet.
// Also, we intentionally use New() with arguments for health::mojom types to
// let the compiler detect untested data members.

TEST(ProbeServiceConvertors, ConvertCategoryVector) {
  const std::vector<health::mojom::ProbeCategoryEnum> kInput{
      health::mojom::ProbeCategoryEnum::kBattery,
      health::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices,
      health::mojom::ProbeCategoryEnum::kCachedVpdData,
      health::mojom::ProbeCategoryEnum::kCpu,
      health::mojom::ProbeCategoryEnum::kTimezone,
      health::mojom::ProbeCategoryEnum::kMemory,
      health::mojom::ProbeCategoryEnum::kBacklight,
      health::mojom::ProbeCategoryEnum::kFan,
      health::mojom::ProbeCategoryEnum::kStatefulPartition,
      health::mojom::ProbeCategoryEnum::kBluetooth};
  EXPECT_THAT(
      ConvertCategoryVector(kInput),
      ElementsAre(
          cros_healthd::mojom::ProbeCategoryEnum::kBattery,
          cros_healthd::mojom::ProbeCategoryEnum::kNonRemovableBlockDevices,
          cros_healthd::mojom::ProbeCategoryEnum::kSystem,
          cros_healthd::mojom::ProbeCategoryEnum::kCpu,
          cros_healthd::mojom::ProbeCategoryEnum::kTimezone,
          cros_healthd::mojom::ProbeCategoryEnum::kMemory,
          cros_healthd::mojom::ProbeCategoryEnum::kBacklight,
          cros_healthd::mojom::ProbeCategoryEnum::kFan,
          cros_healthd::mojom::ProbeCategoryEnum::kStatefulPartition,
          cros_healthd::mojom::ProbeCategoryEnum::kBluetooth));
}

TEST(ProbeServiceConvertors, ErrorType) {
  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kFileReadError),
            health::mojom::ErrorType::kFileReadError);

  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kParseError),
            health::mojom::ErrorType::kParseError);

  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kSystemUtilityError),
            health::mojom::ErrorType::kSystemUtilityError);

  EXPECT_EQ(Convert(cros_healthd::mojom::ErrorType::kServiceUnavailable),
            health::mojom::ErrorType::kServiceUnavailable);
}

TEST(ProbeServiceConvertors, ProbeErrorPtr) {
  constexpr char kMsg[] = "file not found";
  EXPECT_EQ(ConvertPtr(cros_healthd::mojom::ProbeError::New(
                cros_healthd::mojom::ErrorType::kFileReadError, kMsg)),
            health::mojom::ProbeError::New(
                health::mojom::ErrorType::kFileReadError, kMsg));
}

TEST(ProbeServiceConvertors, BoolValue) {
  EXPECT_EQ(Convert(false), health::mojom::BoolValue::New(false));
  EXPECT_EQ(Convert(true), health::mojom::BoolValue::New(true));
}

TEST(ProbeServiceConvertors, DoubleValue) {
  constexpr double kValue = 100500111111.500100;
  EXPECT_EQ(Convert(kValue), health::mojom::DoubleValue::New(kValue));
}

TEST(ProbeServiceConvertors, Int64Value) {
  constexpr int64_t kValue = -(1LL << 62) + 1000;
  EXPECT_EQ(Convert(kValue), health::mojom::Int64Value::New(kValue));
}

TEST(ProbeServiceConvertors, UInt64Value) {
  constexpr uint64_t kValue = (1ULL << 63) + 1000000000;
  EXPECT_EQ(Convert(kValue), health::mojom::UInt64Value::New(kValue));
}

TEST(ProbeServiceConvertors, UInt64ValuePtr) {
  constexpr uint64_t kValue = (1ULL << 63) + 3000000000;
  EXPECT_EQ(ConvertPtr(cros_healthd::mojom::UInt64Value::New(kValue)),
            health::mojom::UInt64Value::New(kValue));
}

TEST(ProbeServiceConvertors, BatteryInfoPtr) {
  constexpr int64_t kCycleCount = (1LL << 62) + 45;
  constexpr double kVoltageNow = 1000000000000.2;
  constexpr char kVendor[] = "Google";
  constexpr char kSerialNumber[] = "ABCDEF123456";
  constexpr double kChargeFullDesign = 10000000000.3;
  constexpr double kChargeFull = 99999999999999.0;
  constexpr double kVoltageMinDesign = 41111111111111.1;
  constexpr char kModelName[] = "Google Battery";
  constexpr double kChargeNow = 200000000000000.1;
  constexpr double kCurrentNow = 1555555555555.2;
  constexpr char kTechnology[] = "FastCharge";
  constexpr char kStatus[] = "Charging";
  constexpr char kManufactureDate[] = "2018-10-01";
  constexpr uint64_t kTemperature = (1ULL << 63) + 9000;

  auto input = cros_healthd::mojom::BatteryInfo::New();
  {
    input->cycle_count = kCycleCount;
    input->voltage_now = kVoltageNow;
    input->vendor = kVendor;
    input->serial_number = kSerialNumber;
    input->charge_full_design = kChargeFullDesign;
    input->charge_full = kChargeFull;
    input->voltage_min_design = kVoltageMinDesign;
    input->model_name = kModelName;
    input->charge_now = kChargeNow;
    input->current_now = kCurrentNow;
    input->technology = kTechnology;
    input->status = kStatus;
    input->manufacture_date = kManufactureDate;
    input->temperature = cros_healthd::mojom::UInt64Value::New(kTemperature);
  }

  EXPECT_EQ(
      ConvertPtr(std::move(input)),
      health::mojom::BatteryInfo::New(
          health::mojom::Int64Value::New(kCycleCount),
          health::mojom::DoubleValue::New(kVoltageNow), kVendor, kSerialNumber,
          health::mojom::DoubleValue::New(kChargeFullDesign),
          health::mojom::DoubleValue::New(kChargeFull),
          health::mojom::DoubleValue::New(kVoltageMinDesign), kModelName,
          health::mojom::DoubleValue::New(kChargeNow),
          health::mojom::DoubleValue::New(kCurrentNow), kTechnology, kStatus,
          kManufactureDate, health::mojom::UInt64Value::New(kTemperature)));
}

TEST(ProbeServiceConvertors, BatteryResultPtrInfo) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::BatteryResult::NewBatteryInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_battery_info());
}

TEST(ProbeServiceConvertors, BatteryResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::BatteryResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, NonRemovableBlockDeviceInfoPtr) {
  constexpr char kPath[] = "/dev/device1";
  constexpr uint64_t kSize = (1ULL << 63) + 111;
  constexpr char kType[] = "NVMe";
  constexpr uint8_t kManufacturerId = 200;
  constexpr char kName[] = "goog";
  constexpr uint32_t kSerial = 4287654321;
  constexpr char kSerialString[] = "4287654321";
  constexpr uint64_t kBytesReadSinceLastBoot = (1ULL << 62) + 222;
  constexpr uint64_t kBytesWrittenSinceLastBoot = (1ULL << 61) + 333;
  constexpr uint64_t kReadTimeSecondsSinceLastBoot = (1ULL << 60) + 444;
  constexpr uint64_t kWriteTimeSecondsSinceLastBoot = (1ULL << 59) + 555;
  constexpr uint64_t kIoTimeSecondsSinceLastBoot = (1ULL << 58) + 666;
  constexpr uint64_t kDiscardTimeSecondsSinceLastBoot = (1ULL << 57) + 777;

  auto input = cros_healthd::mojom::NonRemovableBlockDeviceInfo::New();
  {
    input->path = kPath;
    input->size = kSize;
    input->type = kType;
    input->manufacturer_id = kManufacturerId;
    input->name = kName;
    input->serial = kSerial;
    input->bytes_read_since_last_boot = kBytesReadSinceLastBoot;
    input->bytes_written_since_last_boot = kBytesWrittenSinceLastBoot;
    input->read_time_seconds_since_last_boot = kReadTimeSecondsSinceLastBoot;
    input->write_time_seconds_since_last_boot = kWriteTimeSecondsSinceLastBoot;
    input->io_time_seconds_since_last_boot = kIoTimeSecondsSinceLastBoot;
    input->discard_time_seconds_since_last_boot =
        cros_healthd::mojom::UInt64Value::New(kDiscardTimeSecondsSinceLastBoot);
  }

  EXPECT_EQ(
      ConvertPtr(std::move(input)),
      health::mojom::NonRemovableBlockDeviceInfo::New(
          kPath, health::mojom::UInt64Value::New(kSize), kType,
          health::mojom::UInt32Value::New(kManufacturerId), kName,
          kSerialString,
          health::mojom::UInt64Value::New(kBytesReadSinceLastBoot),
          health::mojom::UInt64Value::New(kBytesWrittenSinceLastBoot),
          health::mojom::UInt64Value::New(kReadTimeSecondsSinceLastBoot),
          health::mojom::UInt64Value::New(kWriteTimeSecondsSinceLastBoot),
          health::mojom::UInt64Value::New(kIoTimeSecondsSinceLastBoot),
          health::mojom::UInt64Value::New(kDiscardTimeSecondsSinceLastBoot)));
}

TEST(ProbeServiceConvertors, NonRemovableBlockDeviceResultPtrInfo) {
  constexpr char kPath1[] = "Path1";
  constexpr char kPath2[] = "Path2";

  std::vector<cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr> infos;
  {
    auto info1 = cros_healthd::mojom::NonRemovableBlockDeviceInfo::New();
    info1->path = kPath1;

    auto info2 = cros_healthd::mojom::NonRemovableBlockDeviceInfo::New();
    info2->path = kPath2;

    infos.push_back(std::move(info1));
    infos.push_back(std::move(info2));
  }

  const auto output = ConvertPtr(
      cros_healthd::mojom::NonRemovableBlockDeviceResult::NewBlockDeviceInfo(
          std::move(infos)));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_block_device_info());
  ASSERT_EQ(output->get_block_device_info().size(), 2ULL);
  EXPECT_EQ(output->get_block_device_info()[0]->path, kPath1);
  EXPECT_EQ(output->get_block_device_info()[1]->path, kPath2);
}

TEST(ProbeServiceConvertors, NonRemovableBlockDeviceResultPtrError) {
  const health::mojom::NonRemovableBlockDeviceResultPtr output = ConvertPtr(
      cros_healthd::mojom::NonRemovableBlockDeviceResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, CachedVpdInfoPtr) {
  constexpr char kSkuNumber[] = "sku-1";

  auto input = cros_healthd::mojom::SystemInfo::New();
  input->product_sku_number = kSkuNumber;

  EXPECT_EQ(ConvertPtr(std::move(input)),
            health::mojom::CachedVpdInfo::New(kSkuNumber));
}

TEST(ProbeServiceConvertors, CachedVpdResultPtrInfo) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::SystemResult::NewSystemInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_vpd_info());
}

TEST(ProbeServiceConvertors, CachedVpdResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::SystemResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, CpuCStateInfoPtr) {
  constexpr char kName[] = "C0";
  constexpr uint64_t kTimeInStateSinceLastBootUs = 123456;

  auto input = cros_healthd::mojom::CpuCStateInfo::New();
  {
    input->name = kName;
    input->time_in_state_since_last_boot_us = kTimeInStateSinceLastBootUs;
  }

  EXPECT_EQ(
      ConvertPtr(std::move(input)),
      health::mojom::CpuCStateInfo::New(
          kName, health::mojom::UInt64Value::New(kTimeInStateSinceLastBootUs)));
}

TEST(ProbeServiceConvertors, LogicalCpuInfoPtr) {
  constexpr uint32_t kMaxClockSpeedKhz = (1 << 31) + 10000;
  constexpr uint32_t kScalingMaxFrequencyKhz = (1 << 30) + 20000;
  constexpr uint32_t kScalingCurrentFrequencyKhz = (1 << 29) + 30000;

  // Idle time cannot be tested with ConvertPtr, because it requires USER_HZ
  // system constant to convert idle_time_user_hz to milliseconds.
  constexpr uint32_t kIdleTime = 0;

  constexpr char kCpuCStateName[] = "C1";
  constexpr uint64_t kCpuCStateTime = (1 << 27) + 50000;

  auto input = cros_healthd::mojom::LogicalCpuInfo::New();
  {
    auto c_state = cros_healthd::mojom::CpuCStateInfo::New();
    c_state->name = kCpuCStateName;
    c_state->time_in_state_since_last_boot_us = kCpuCStateTime;

    input->max_clock_speed_khz = kMaxClockSpeedKhz;
    input->scaling_max_frequency_khz = kScalingMaxFrequencyKhz;
    input->scaling_current_frequency_khz = kScalingCurrentFrequencyKhz;
    input->idle_time_user_hz = kIdleTime;
    input->c_states.push_back(std::move(c_state));
  }

  std::vector<health::mojom::CpuCStateInfoPtr> expected_c_states;
  expected_c_states.push_back(health::mojom::CpuCStateInfo::New(
      kCpuCStateName, health::mojom::UInt64Value::New(kCpuCStateTime)));

  EXPECT_EQ(ConvertPtr(std::move(input)),
            health::mojom::LogicalCpuInfo::New(
                health::mojom::UInt32Value::New(kMaxClockSpeedKhz),
                health::mojom::UInt32Value::New(kScalingMaxFrequencyKhz),
                health::mojom::UInt32Value::New(kScalingCurrentFrequencyKhz),
                health::mojom::UInt64Value::New(kIdleTime),
                std::move(expected_c_states)));
}

TEST(ProbeServiceConvertors, LogicalCpuInfoPtrNonZeroIdleTime) {
  constexpr uint64_t kUserHz = 100;
  constexpr uint32_t kIdleTimeUserHz = 4291234295;
  constexpr uint64_t kIdleTimeMs = 42912342950;

  auto input = cros_healthd::mojom::LogicalCpuInfo::New();
  input->idle_time_user_hz = kIdleTimeUserHz;

  const auto output = unchecked::UncheckedConvertPtr(std::move(input), kUserHz);
  ASSERT_TRUE(output);
  EXPECT_EQ(output->idle_time_ms, health::mojom::UInt64Value::New(kIdleTimeMs));
}

TEST(ProbeServiceConvertors, PhysicalCpuInfoPtr) {
  constexpr char kModelName[] = "i9";

  constexpr uint32_t kMaxClockSpeedKhz = (1 << 31) + 11111;
  constexpr uint32_t kScalingMaxFrequencyKhz = (1 << 30) + 22222;
  constexpr uint32_t kScalingCurrentFrequencyKhz = (1 << 29) + 33333;

  // Idle time cannot be tested with ConvertPtr, because it requires USER_HZ
  // system constant to convert idle_time_user_hz to milliseconds.
  constexpr uint32_t kIdleTime = 0;

  auto input = cros_healthd::mojom::PhysicalCpuInfo::New();
  {
    auto logical_info = cros_healthd::mojom::LogicalCpuInfo::New();
    logical_info->max_clock_speed_khz = kMaxClockSpeedKhz;
    logical_info->scaling_max_frequency_khz = kScalingMaxFrequencyKhz;
    logical_info->scaling_current_frequency_khz = kScalingCurrentFrequencyKhz;
    logical_info->idle_time_user_hz = kIdleTime;

    input->model_name = kModelName;
    input->logical_cpus.push_back(std::move(logical_info));
  }

  std::vector<health::mojom::LogicalCpuInfoPtr> expected_infos;
  expected_infos.push_back(health::mojom::LogicalCpuInfo::New(
      health::mojom::UInt32Value::New(kMaxClockSpeedKhz),
      health::mojom::UInt32Value::New(kScalingMaxFrequencyKhz),
      health::mojom::UInt32Value::New(kScalingCurrentFrequencyKhz),
      health::mojom::UInt64Value::New(kIdleTime),
      std::vector<health::mojom::CpuCStateInfoPtr>{}));

  EXPECT_EQ(ConvertPtr(std::move(input)),
            health::mojom::PhysicalCpuInfo::New(kModelName,
                                                std::move(expected_infos)));
}

TEST(ProbeServiceConvertors, CpuArchitectureEnum) {
  EXPECT_EQ(Convert(cros_healthd::mojom::CpuArchitectureEnum::kUnknown),
            health::mojom::CpuArchitectureEnum::kUnknown);
  EXPECT_EQ(Convert(cros_healthd::mojom::CpuArchitectureEnum::kX86_64),
            health::mojom::CpuArchitectureEnum::kX86_64);
  EXPECT_EQ(Convert(cros_healthd::mojom::CpuArchitectureEnum::kAArch64),
            health::mojom::CpuArchitectureEnum::kAArch64);
  EXPECT_EQ(Convert(cros_healthd::mojom::CpuArchitectureEnum::kArmv7l),
            health::mojom::CpuArchitectureEnum::kArmv7l);
}

TEST(ProbeServiceConvertors, CpuInfoPtr) {
  constexpr uint32_t kNumTotalThreads = (1 << 31) + 111;
  constexpr char kModelName[] = "i9";

  auto input = cros_healthd::mojom::CpuInfo::New();
  {
    auto physical_info = cros_healthd::mojom::PhysicalCpuInfo::New();
    physical_info->model_name = kModelName;

    input->num_total_threads = kNumTotalThreads;
    input->architecture = cros_healthd::mojom::CpuArchitectureEnum::kArmv7l;
    input->physical_cpus.push_back(std::move(physical_info));
  }

  std::vector<health::mojom::PhysicalCpuInfoPtr> expected_infos;
  expected_infos.push_back(health::mojom::PhysicalCpuInfo::New(
      kModelName, std::vector<health::mojom::LogicalCpuInfoPtr>{}));

  EXPECT_EQ(ConvertPtr(std::move(input)),
            health::mojom::CpuInfo::New(
                health::mojom::UInt32Value::New(kNumTotalThreads),
                health::mojom::CpuArchitectureEnum::kArmv7l,
                std::move(expected_infos)));
}

TEST(ProbeServiceConvertors, CpuResultPtrInfo) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::CpuResult::NewCpuInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_cpu_info());
}

TEST(ProbeServiceConvertors, CpuResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::CpuResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, TimezoneInfoPtr) {
  constexpr char kPosix[] = "TZ=CST6CDT,M3.2.0/2:00:00,M11.1.0/2:00:00";
  constexpr char kRegion[] = "Europe/Berlin";

  auto input = cros_healthd::mojom::TimezoneInfo::New();
  input->posix = kPosix;
  input->region = kRegion;

  EXPECT_EQ(ConvertPtr(std::move(input)),
            health::mojom::TimezoneInfo::New(kPosix, kRegion));
}

TEST(ProbeServiceConvertors, TimezoneResultPtrInfo) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::TimezoneResult::NewTimezoneInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_timezone_info());
}

TEST(ProbeServiceConvertors, TimezoneResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::TimezoneResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, MemoryInfoPtr) {
  constexpr uint32_t kTotalMemoryKib = (1 << 31) + 100;
  constexpr uint32_t kFreeMemoryKib = (1 << 30) + 200;
  constexpr uint32_t kAvailableMemoryKib = (1 << 29) + 300;
  constexpr uint32_t kPageFaultsSinceLastBoot = (1 << 28) + 400;

  auto input = cros_healthd::mojom::MemoryInfo::New();
  input->total_memory_kib = kTotalMemoryKib;
  input->free_memory_kib = kFreeMemoryKib;
  input->available_memory_kib = kAvailableMemoryKib;
  input->page_faults_since_last_boot = kPageFaultsSinceLastBoot;

  EXPECT_EQ(ConvertPtr(std::move(input)),
            health::mojom::MemoryInfo::New(
                health::mojom::UInt32Value::New(kTotalMemoryKib),
                health::mojom::UInt32Value::New(kFreeMemoryKib),
                health::mojom::UInt32Value::New(kAvailableMemoryKib),
                health::mojom::UInt64Value::New(kPageFaultsSinceLastBoot)));
}

TEST(ProbeServiceConvertors, MemoryResultPtrInfo) {
  const health::mojom::MemoryResultPtr output =
      ConvertPtr(cros_healthd::mojom::MemoryResult::NewMemoryInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_memory_info());
}

TEST(ProbeServiceConvertors, MemoryResultPtrError) {
  const health::mojom::MemoryResultPtr output =
      ConvertPtr(cros_healthd::mojom::MemoryResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, BacklightInfoPtr) {
  constexpr char kPath[] = "/sys/backlight";
  constexpr uint32_t kMaxBrightness = (1 << 31) + 31;
  constexpr uint32_t kBrightness = (1 << 30) + 30;

  auto input = cros_healthd::mojom::BacklightInfo::New();
  input->path = kPath;
  input->max_brightness = kMaxBrightness;
  input->brightness = kBrightness;

  EXPECT_EQ(ConvertPtr(std::move(input)),
            health::mojom::BacklightInfo::New(
                kPath, health::mojom::UInt32Value::New(kMaxBrightness),
                health::mojom::UInt32Value::New(kBrightness)));
}

TEST(ProbeServiceConvertors, BacklightResultPtrInfo) {
  constexpr char kPath[] = "/sys/backlight";

  cros_healthd::mojom::BacklightResultPtr input;
  {
    auto backlight_info = cros_healthd::mojom::BacklightInfo::New();
    backlight_info->path = kPath;

    std::vector<cros_healthd::mojom::BacklightInfoPtr> backlight_infos;
    backlight_infos.push_back(std::move(backlight_info));

    input = cros_healthd::mojom::BacklightResult::NewBacklightInfo(
        std::move(backlight_infos));
  }

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  ASSERT_TRUE(output->is_backlight_info());

  const auto& backlight_info_output = output->get_backlight_info();
  ASSERT_EQ(backlight_info_output.size(), 1ULL);
  ASSERT_TRUE(backlight_info_output[0]);
  EXPECT_EQ(backlight_info_output[0]->path, kPath);
}

TEST(ProbeServiceConvertors, BacklightResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::BacklightResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, FanInfoPtr) {
  constexpr uint32_t kSpeedRpm = (1 << 31) + 777;

  auto input = cros_healthd::mojom::FanInfo::New();
  input->speed_rpm = kSpeedRpm;

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  EXPECT_EQ(output->speed_rpm, health::mojom::UInt32Value::New(kSpeedRpm));
}

TEST(ProbeServiceConvertors, FanResultPtrInfo) {
  constexpr uint32_t kSpeedRpm = (1 << 31) + 10;

  cros_healthd::mojom::FanResultPtr input;
  {
    auto fan_info = cros_healthd::mojom::FanInfo::New();
    fan_info->speed_rpm = kSpeedRpm;

    std::vector<cros_healthd::mojom::FanInfoPtr> fan_infos;
    fan_infos.push_back(std::move(fan_info));

    input = cros_healthd::mojom::FanResult::NewFanInfo(std::move(fan_infos));
  }

  std::vector<health::mojom::FanInfoPtr> expected_fans;
  expected_fans.push_back(
      health::mojom::FanInfo::New(health::mojom::UInt32Value::New(kSpeedRpm)));

  EXPECT_EQ(ConvertPtr(std::move(input)),
            health::mojom::FanResult::NewFanInfo(std::move(expected_fans)));
}

TEST(ProbeServiceConvertors, FanResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::FanResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, StatefulPartitionInfoPtr) {
  constexpr uint64_t k100MiB = 100 * 1024 * 1024;
  constexpr uint64_t kTotalSpace = 9000000 * k100MiB + 17;
  constexpr uint64_t kRoundedAvailableSpace = 800000 * k100MiB;
  constexpr uint64_t kAvailableSpace = kRoundedAvailableSpace + k100MiB - 2000;

  auto input = cros_healthd::mojom::StatefulPartitionInfo::New();
  input->available_space = kAvailableSpace;
  input->total_space = kTotalSpace;

  EXPECT_EQ(ConvertPtr(std::move(input)),
            health::mojom::StatefulPartitionInfo::New(
                health::mojom::UInt64Value::New(kRoundedAvailableSpace),
                health::mojom::UInt64Value::New(kTotalSpace)));
}

TEST(ProbeServiceConvertors, StatefulPartitionResultPtrInfo) {
  const auto output = ConvertPtr(
      cros_healthd::mojom::StatefulPartitionResult::NewPartitionInfo(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_partition_info());
}

TEST(ProbeServiceConvertors, StatefulPartitionResultPtrError) {
  const auto output = ConvertPtr(
      cros_healthd::mojom::StatefulPartitionResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, BluetoothAdapterInfoPtr) {
  constexpr char kName[] = "hci0";
  constexpr char kAddress[] = "ab:cd:ef:12:34:56";
  constexpr bool kPowered = true;
  constexpr uint32_t kNumConnectedDevices = (1 << 30) + 1;

  auto input = cros_healthd::mojom::BluetoothAdapterInfo::New();
  {
    input->name = kName;
    input->address = kAddress;
    input->powered = kPowered;
    input->num_connected_devices = kNumConnectedDevices;
  }

  EXPECT_EQ(ConvertPtr(std::move(input)),
            health::mojom::BluetoothAdapterInfo::New(
                kName, kAddress, health::mojom::BoolValue::New(kPowered),
                health::mojom::UInt32Value::New(kNumConnectedDevices)));
}

TEST(ProbeServiceConvertors, BluetoothResultPtrInfo) {
  constexpr char kName[] = "hci0";

  cros_healthd::mojom::BluetoothResultPtr input;
  {
    auto info = cros_healthd::mojom::BluetoothAdapterInfo::New();
    info->name = kName;

    std::vector<cros_healthd::mojom::BluetoothAdapterInfoPtr> infos;
    infos.push_back(std::move(info));

    input = cros_healthd::mojom::BluetoothResult::NewBluetoothAdapterInfo(
        std::move(infos));
  }

  const auto output = ConvertPtr(std::move(input));
  ASSERT_TRUE(output);
  ASSERT_TRUE(output->is_bluetooth_adapter_info());

  const auto& bluetooth_adapter_info_output =
      output->get_bluetooth_adapter_info();
  ASSERT_EQ(bluetooth_adapter_info_output.size(), 1ULL);
  ASSERT_TRUE(bluetooth_adapter_info_output[0]);
  EXPECT_EQ(bluetooth_adapter_info_output[0]->name, kName);
}

TEST(ProbeServiceConvertors, BluetoothResultPtrError) {
  const auto output =
      ConvertPtr(cros_healthd::mojom::BluetoothResult::NewError(nullptr));
  ASSERT_TRUE(output);
  EXPECT_TRUE(output->is_error());
}

TEST(ProbeServiceConvertors, TelemetryInfoPtrWithNotNullFields) {
  auto input = cros_healthd::mojom::TelemetryInfo::New();
  {
    input->battery_result = cros_healthd::mojom::BatteryResult::New();
    input->block_device_result =
        cros_healthd::mojom::NonRemovableBlockDeviceResult::New();
    input->system_result = cros_healthd::mojom::SystemResult::New();
    input->cpu_result = cros_healthd::mojom::CpuResult::New();
    input->timezone_result = cros_healthd::mojom::TimezoneResult::New();
    input->memory_result = cros_healthd::mojom::MemoryResult::New();
    input->backlight_result = cros_healthd::mojom::BacklightResult::New();
    input->fan_result = cros_healthd::mojom::FanResult::New();
    input->stateful_partition_result =
        cros_healthd::mojom::StatefulPartitionResult::New();
    input->bluetooth_result = cros_healthd::mojom::BluetoothResult::New();
  }

  EXPECT_EQ(
      ConvertPtr(std::move(input)),
      health::mojom::TelemetryInfo::New(
          health::mojom::BatteryResult::New(),
          health::mojom::NonRemovableBlockDeviceResult::New(),
          health::mojom::CachedVpdResult::New(),
          health::mojom::CpuResult::New(), health::mojom::TimezoneResult::New(),
          health::mojom::MemoryResult::New(),
          health::mojom::BacklightResult::New(),
          health::mojom::FanResult::New(),
          health::mojom::StatefulPartitionResult::New(),
          health::mojom::BluetoothResult::New()));
}

TEST(ProbeServiceConvertors, TelemetryInfoPtrWithNullFields) {
  EXPECT_EQ(ConvertPtr(cros_healthd::mojom::TelemetryInfo::New()),
            health::mojom::TelemetryInfo::New(
                health::mojom::BatteryResultPtr(nullptr),
                health::mojom::NonRemovableBlockDeviceResultPtr(nullptr),
                health::mojom::CachedVpdResultPtr(nullptr),
                health::mojom::CpuResultPtr(nullptr),
                health::mojom::TimezoneResultPtr(nullptr),
                health::mojom::MemoryResultPtr(nullptr),
                health::mojom::BacklightResultPtr(nullptr),
                health::mojom::FanResultPtr(nullptr),
                health::mojom::StatefulPartitionResultPtr(nullptr),
                health::mojom::BluetoothResultPtr(nullptr)));
}

}  // namespace converters
}  // namespace chromeos
