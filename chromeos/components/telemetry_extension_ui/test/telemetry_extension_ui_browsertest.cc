// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/test/telemetry_extension_ui_browsertest.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/wilco_dtc_supportd/mojo_utils.h"
#include "chromeos/components/telemetry_extension_ui/url_constants.h"
#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"

namespace {

// File with utility functions for testing, defines `test_util`.
constexpr base::FilePath::CharType kWebUiTestUtil[] =
    FILE_PATH_LITERAL("chrome/test/data/webui/test_util.js");

// File that `kWebUiTestUtil` is dependent on, defines `cr`.
constexpr base::FilePath::CharType kCr[] =
    FILE_PATH_LITERAL("ui/webui/resources/js/cr.js");

// Folder containing the resources for JS browser tests.
constexpr base::FilePath::CharType kUntrustedAppResources[] = FILE_PATH_LITERAL(
    "chromeos/components/telemetry_extension_ui/test/untrusted_app_resources");

// File containing the query handlers for JS unit tests.
constexpr base::FilePath::CharType kUntrustedTestHandlers[] = FILE_PATH_LITERAL(
    "chromeos/components/telemetry_extension_ui/test/"
    "untrusted_test_handlers.js");

// Test cases that run in the untrusted context.
constexpr base::FilePath::CharType kUntrustedTestCases[] = FILE_PATH_LITERAL(
    "chromeos/components/telemetry_extension_ui/test/untrusted_browsertest.js");

}  // namespace

TelemetryExtensionUiBrowserTest::TelemetryExtensionUiBrowserTest()
    : SandboxedWebUiAppTestBase(
          chromeos::kChromeUITelemetryExtensionURL,
          chromeos::kChromeUIUntrustedTelemetryExtensionURL,
          {base::FilePath(kCr), base::FilePath(kWebUiTestUtil),
           base::FilePath(kUntrustedTestHandlers),
           base::FilePath(kUntrustedTestCases)}) {}

TelemetryExtensionUiBrowserTest::~TelemetryExtensionUiBrowserTest() = default;

void TelemetryExtensionUiBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  base::FilePath source_root;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root);
  base::FilePath file_path(kUntrustedAppResources);

  command_line->AppendSwitchASCII(
      chromeos::switches::kTelemetryExtensionDirectory,
      source_root.Append(file_path).value());

  SandboxedWebUiAppTestBase::SetUpCommandLine(command_line);
}

void TelemetryExtensionUiBrowserTest::SetUpOnMainThread() {
  {
    namespace cros_diagnostics = ::chromeos::cros_healthd::mojom;

    std::vector<cros_diagnostics::DiagnosticRoutineEnum> input{
        cros_diagnostics::DiagnosticRoutineEnum::kBatteryCapacity,
        cros_diagnostics::DiagnosticRoutineEnum::kBatteryHealth,
        cros_diagnostics::DiagnosticRoutineEnum::kSmartctlCheck,
        cros_diagnostics::DiagnosticRoutineEnum::kAcPower,
        cros_diagnostics::DiagnosticRoutineEnum::kCpuCache,
        cros_diagnostics::DiagnosticRoutineEnum::kCpuStress,
        cros_diagnostics::DiagnosticRoutineEnum::kFloatingPointAccuracy,
        cros_diagnostics::DiagnosticRoutineEnum::kNvmeWearLevel,
        cros_diagnostics::DiagnosticRoutineEnum::kNvmeSelfTest,
        cros_diagnostics::DiagnosticRoutineEnum::kDiskRead,
        cros_diagnostics::DiagnosticRoutineEnum::kPrimeSearch,
        cros_diagnostics::DiagnosticRoutineEnum::kBatteryDischarge,
        cros_diagnostics::DiagnosticRoutineEnum::kBatteryCharge,
    };

    chromeos::cros_healthd::FakeCrosHealthdClient::Get()
        ->SetAvailableRoutinesForTesting(input);
  }

  {
    auto input = chromeos::cros_healthd::mojom::RunRoutineResponse::New();
    input->id = 123456789;
    input->status =
        chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum::kReady;

    chromeos::cros_healthd::FakeCrosHealthdClient::Get()
        ->SetRunRoutineResponseForTesting(input);
  }

  auto telemetry_info = chromeos::cros_healthd::mojom::TelemetryInfo::New();
  {
    auto battery_info = chromeos::cros_healthd::mojom::BatteryInfo::New();
    battery_info->cycle_count = 100000000000000;
    battery_info->voltage_now = 1234567890.123456;
    battery_info->vendor = "Google";
    battery_info->serial_number = "abcdef";
    battery_info->charge_full_design = 3000000000000000;
    battery_info->charge_full = 9000000000000000;
    battery_info->voltage_min_design = 1000000000.1001;
    battery_info->model_name = "Google Battery";
    battery_info->charge_now = 7777777777.777;
    battery_info->current_now = 0.9999999999999;
    battery_info->technology = "Li-ion";
    battery_info->status = "Charging";
    battery_info->manufacture_date = "2020-07-30";
    battery_info->temperature =
        chromeos::cros_healthd::mojom::UInt64Value::New(7777777777777777);

    telemetry_info->battery_result =
        chromeos::cros_healthd::mojom::BatteryResult::NewBatteryInfo(
            std::move(battery_info));
  }
  {
    auto block_device_info =
        chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfo::New();
    block_device_info->path = "/dev/device1";
    block_device_info->size = 5555555555555555;
    block_device_info->type = "NVMe";
    block_device_info->manufacturer_id = 200;
    block_device_info->name = "goog";
    block_device_info->serial = 4287654321;
    block_device_info->bytes_read_since_last_boot = 9000000000000000;
    block_device_info->bytes_written_since_last_boot = 8000000000000000;
    block_device_info->read_time_seconds_since_last_boot = 7000000000000000;
    block_device_info->write_time_seconds_since_last_boot = 6666666666666666;
    block_device_info->io_time_seconds_since_last_boot = 1111111111111;
    block_device_info->discard_time_seconds_since_last_boot =
        chromeos::cros_healthd::mojom::UInt64Value::New(77777777777777);

    // Need to put some placeholder values, otherwise Mojo will crash, because
    // mandatory union fields cannot be nullptr.
    block_device_info->vendor_id =
        chromeos::cros_healthd::mojom::BlockDeviceVendor::NewOther(0);
    block_device_info->product_id =
        chromeos::cros_healthd::mojom::BlockDeviceProduct::NewOther(0);
    block_device_info->revision =
        chromeos::cros_healthd::mojom::BlockDeviceRevision::NewOther(0);
    block_device_info->firmware_version =
        chromeos::cros_healthd::mojom::BlockDeviceFirmware::NewOther(0);

    std::vector<chromeos::cros_healthd::mojom::NonRemovableBlockDeviceInfoPtr>
        infos;
    infos.push_back(std::move(block_device_info));

    telemetry_info->block_device_result = chromeos::cros_healthd::mojom::
        NonRemovableBlockDeviceResult::NewBlockDeviceInfo(std::move(infos));
  }
  {
    auto os_version = chromeos::cros_healthd::mojom::OsVersion::New();
    os_version->release_milestone = "87";
    os_version->build_number = "13544";
    os_version->patch_number = "59.0";
    os_version->release_channel = "stable-channel";

    auto system_info = chromeos::cros_healthd::mojom::SystemInfo::New();
    system_info->product_sku_number = "sku-18";
    system_info->os_version = std::move(os_version);

    telemetry_info->system_result =
        chromeos::cros_healthd::mojom::SystemResult::NewSystemInfo(
            std::move(system_info));
  }
  {
    auto c_state1 = chromeos::cros_healthd::mojom::CpuCStateInfo::New();
    c_state1->name = "C1";
    c_state1->time_in_state_since_last_boot_us = 1125899906875957;

    auto c_state2 = chromeos::cros_healthd::mojom::CpuCStateInfo::New();
    c_state2->name = "C2";
    c_state2->time_in_state_since_last_boot_us = 1125899906877777;

    auto logical_info1 = chromeos::cros_healthd::mojom::LogicalCpuInfo::New();
    logical_info1->max_clock_speed_khz = 2147494759;
    logical_info1->scaling_max_frequency_khz = 1073764046;
    logical_info1->scaling_current_frequency_khz = 536904245;
    logical_info1->c_states.push_back(std::move(c_state1));
    logical_info1->c_states.push_back(std::move(c_state2));
    // Idle time cannot be tested in browser test, because it requires USER_HZ
    // system constant to convert idle_time_user_hz to milliseconds.
    logical_info1->idle_time_user_hz = 0;

    auto logical_info2 = chromeos::cros_healthd::mojom::LogicalCpuInfo::New();
    logical_info2->max_clock_speed_khz = 1147494759;
    logical_info2->scaling_max_frequency_khz = 1063764046;
    logical_info2->scaling_current_frequency_khz = 936904246;
    // Idle time cannot be tested in browser test, because it requires USER_HZ
    // system constant to convert idle_time_user_hz to milliseconds.
    logical_info2->idle_time_user_hz = 0;

    auto physical_info1 = chromeos::cros_healthd::mojom::PhysicalCpuInfo::New();
    physical_info1->model_name = "i9";
    physical_info1->logical_cpus.push_back(std::move(logical_info1));
    physical_info1->logical_cpus.push_back(std::move(logical_info2));

    auto physical_info2 = chromeos::cros_healthd::mojom::PhysicalCpuInfo::New();
    physical_info2->model_name = "i9-low-powered";

    auto cpu_info = chromeos::cros_healthd::mojom::CpuInfo::New();
    cpu_info->num_total_threads = 2147483759;
    cpu_info->architecture =
        chromeos::cros_healthd::mojom::CpuArchitectureEnum::kArmv7l;
    cpu_info->physical_cpus.push_back(std::move(physical_info1));
    cpu_info->physical_cpus.push_back(std::move(physical_info2));

    telemetry_info->cpu_result =
        chromeos::cros_healthd::mojom::CpuResult::NewCpuInfo(
            std::move(cpu_info));
  }
  {
    auto timezone_info = chromeos::cros_healthd::mojom::TimezoneInfo::New();
    timezone_info->posix = "MST7MDT,M3.2.0,M11.1.0";
    timezone_info->region = "America/Denver";

    telemetry_info->timezone_result =
        chromeos::cros_healthd::mojom::TimezoneResult::NewTimezoneInfo(
            std::move(timezone_info));
  }
  {
    auto memory_info = chromeos::cros_healthd::mojom::MemoryInfo::New();
    memory_info->total_memory_kib = 2147483648;
    memory_info->free_memory_kib = 2147573648;
    memory_info->available_memory_kib = 2147571148;
    memory_info->page_faults_since_last_boot = 2199971148;

    telemetry_info->memory_result =
        chromeos::cros_healthd::mojom::MemoryResult::NewMemoryInfo(
            std::move(memory_info));
  }
  {
    auto backlight_info = chromeos::cros_healthd::mojom::BacklightInfo::New();
    backlight_info->path = "/sys/backlight";
    backlight_info->max_brightness = 536880912;
    backlight_info->brightness = 436880912;

    std::vector<chromeos::cros_healthd::mojom::BacklightInfoPtr> infos;
    infos.push_back(std::move(backlight_info));

    telemetry_info->backlight_result =
        chromeos::cros_healthd::mojom::BacklightResult::NewBacklightInfo(
            std::move(infos));
  }
  {
    auto fan_info = chromeos::cros_healthd::mojom::FanInfo::New();
    fan_info->speed_rpm = 999880912;

    std::vector<chromeos::cros_healthd::mojom::FanInfoPtr> infos;
    infos.push_back(std::move(fan_info));

    telemetry_info->fan_result =
        chromeos::cros_healthd::mojom::FanResult::NewFanInfo(std::move(infos));
  }
  {
    auto partition_info =
        chromeos::cros_healthd::mojom::StatefulPartitionInfo::New();
    partition_info->available_space = 1125899906842624;
    partition_info->total_space = 1125900006842624;

    telemetry_info->stateful_partition_result = chromeos::cros_healthd::mojom::
        StatefulPartitionResult::NewPartitionInfo(std::move(partition_info));
  }
  {
    auto bluetooth_info =
        chromeos::cros_healthd::mojom::BluetoothAdapterInfo::New();
    bluetooth_info->name = "hci0";
    bluetooth_info->address = "ab:cd:ef:12:34:56";
    bluetooth_info->powered = true;
    bluetooth_info->num_connected_devices = 4294967295;

    std::vector<chromeos::cros_healthd::mojom::BluetoothAdapterInfoPtr> infos;
    infos.push_back(std::move(bluetooth_info));

    telemetry_info->bluetooth_result =
        chromeos::cros_healthd::mojom::BluetoothResult::NewBluetoothAdapterInfo(
            std::move(infos));
  }

  DCHECK(chromeos::cros_healthd::FakeCrosHealthdClient::Get());

  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);

  ConfigureSystemEventsServiceToEmitEvents();

  SandboxedWebUiAppTestBase::SetUpOnMainThread();
}

void TelemetryExtensionUiBrowserTest::
    ConfigureDiagnosticsForInteractiveUpdate() {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;

  auto input = cros_healthd::RoutineUpdate::New();
  auto routineUpdateUnion = cros_healthd::RoutineUpdateUnion::New();
  auto interactiveRoutineUpdate = cros_healthd::InteractiveRoutineUpdate::New();

  interactiveRoutineUpdate->user_message =
      cros_healthd::DiagnosticRoutineUserMessageEnum::kUnplugACPower;

  routineUpdateUnion->set_interactive_update(
      std::move(interactiveRoutineUpdate));

  input->progress_percent = 0;
  input->output = chromeos::MojoUtils::CreateReadOnlySharedMemoryMojoHandle(
      "This routine is running!");
  input->routine_update_union = std::move(routineUpdateUnion);

  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetGetRoutineUpdateResponseForTesting(input);
}

void TelemetryExtensionUiBrowserTest::
    ConfigureDiagnosticsForNonInteractiveUpdate() {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;

  auto input = cros_healthd::RoutineUpdate::New();
  auto routineUpdateUnion = cros_healthd::RoutineUpdateUnion::New();
  auto nonInteractiveRoutineUpdate =
      cros_healthd::NonInteractiveRoutineUpdate::New();

  nonInteractiveRoutineUpdate->status =
      cros_healthd::DiagnosticRoutineStatusEnum::kReady;
  nonInteractiveRoutineUpdate->status_message = "Routine ran by Google.";

  routineUpdateUnion->set_noninteractive_update(
      std::move(nonInteractiveRoutineUpdate));

  input->progress_percent = 3147483771;
  input->routine_update_union = std::move(routineUpdateUnion);

  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetGetRoutineUpdateResponseForTesting(input);
}

void TelemetryExtensionUiBrowserTest::ConfigureProbeServiceToReturnErrors() {
  namespace cros_healthd = ::chromeos::cros_healthd::mojom;

  auto telemetry_info = cros_healthd::TelemetryInfo::New();
  {
    auto error = cros_healthd::ProbeError::New();
    error->type = cros_healthd::ErrorType::kFileReadError;
    error->msg = "battery error";

    telemetry_info->battery_result =
        cros_healthd::BatteryResult::NewError(std::move(error));
  }
  {
    auto error = cros_healthd::ProbeError::New();
    error->type = cros_healthd::ErrorType::kParseError;
    error->msg = "block device error";

    telemetry_info->block_device_result =
        cros_healthd::NonRemovableBlockDeviceResult::NewError(std::move(error));
  }
  {
    auto error = cros_healthd::ProbeError::New();
    error->type = cros_healthd::ErrorType::kSystemUtilityError;
    error->msg = "vpd error";

    telemetry_info->system_result =
        cros_healthd::SystemResult::NewError(std::move(error));
  }
  {
    auto error = cros_healthd::ProbeError::New();
    error->type = cros_healthd::ErrorType::kServiceUnavailable;
    error->msg = "cpu error";

    telemetry_info->cpu_result =
        cros_healthd::CpuResult::NewError(std::move(error));
  }
  {
    auto error = cros_healthd::ProbeError::New();
    error->type = cros_healthd::ErrorType::kFileReadError;
    error->msg = "timezone error";

    telemetry_info->timezone_result =
        cros_healthd::TimezoneResult::NewError(std::move(error));
  }
  {
    auto error = cros_healthd::ProbeError::New();
    error->type = cros_healthd::ErrorType::kParseError;
    error->msg = "memory error";

    telemetry_info->memory_result =
        cros_healthd::MemoryResult::NewError(std::move(error));
  }
  {
    auto error = cros_healthd::ProbeError::New();
    error->type = cros_healthd::ErrorType::kSystemUtilityError;
    error->msg = "backlight error";

    telemetry_info->backlight_result =
        cros_healthd::BacklightResult::NewError(std::move(error));
  }
  {
    auto error = cros_healthd::ProbeError::New();
    error->type = cros_healthd::ErrorType::kServiceUnavailable;
    error->msg = "fan error";

    telemetry_info->fan_result =
        cros_healthd::FanResult::NewError(std::move(error));
  }
  {
    auto error = cros_healthd::ProbeError::New();
    error->type = cros_healthd::ErrorType::kFileReadError;
    error->msg = "partition error";

    telemetry_info->stateful_partition_result =
        cros_healthd::StatefulPartitionResult::NewError(std::move(error));
  }
  {
    auto error = cros_healthd::ProbeError::New();
    error->type = cros_healthd::ErrorType::kParseError;
    error->msg = "bluetooth error";

    telemetry_info->bluetooth_result =
        cros_healthd::BluetoothResult::NewError(std::move(error));
  }

  DCHECK(chromeos::cros_healthd::FakeCrosHealthdClient::Get());

  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
}

void TelemetryExtensionUiBrowserTest::
    ConfigureSystemEventsServiceToEmitEvents() {
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->EmitLidClosedEventForTesting();
  chromeos::cros_healthd::FakeCrosHealthdClient::Get()
      ->EmitLidOpenedEventForTesting();

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TelemetryExtensionUiBrowserTest::
                         ConfigureSystemEventsServiceToEmitEvents,
                     system_events_weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(1));
}
