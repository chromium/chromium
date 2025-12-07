// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/system_profile_metadata_recorder.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/trace_time.h"
#include "components/tracing/common/background_tracing_metrics_provider.h"
#include "services/tracing/public/cpp/perfetto/metadata_data_source.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "third_party/perfetto/protos/perfetto/common/data_source_descriptor.gen.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/chrome_config.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/extension_descriptor.pbzero.h"

namespace tracing {
namespace {

inline constexpr char kAccessibilityEnabledModesMetadataKey[] =
    "accessibility-enabled-modes";
inline constexpr char kAntivirusProductMetadataPrefix[] = "antivirus-product-";
inline constexpr char kAppLocaleMetadataKey[] = "app-locale";
inline constexpr char kChannelMetadataKey[] = "channel";
inline constexpr char kCpuCoresMetadataKey[] = "cpu-num-cores";
inline constexpr char kCpuEfficientCoresMetadataKey[] =
    "cpu-num-efficient-cores";
inline constexpr char kCpuIsHypervisorMetadataKey[] = "cpu-running-in-vm";
inline constexpr char kCpuSignatureMetadataKey[] = "cpu-signature";
inline constexpr char kCpuVenderMetadataKey[] = "cpu-vendor";
inline constexpr char kDriveHasSeekPenaltyKey[] = "drive-has_seek_penalty";
inline constexpr char kFullHardwareClassMetadataKey[] = "full-hardware-class";
inline constexpr char kGpuDriverVersionMetadataKey[] = "gpu-driver-version";
inline constexpr char kGpuGlRendererMetadataKey[] = "gpu-gl-renderer";
inline constexpr char kGpuGlVendorMetadataKey[] = "gpu-gl-vendor";
inline constexpr char kGpuVendorIdMetadataKey[] = "gpu-vendor-id";
inline constexpr char kHardwareClassMetadataKey[] = "hardware-class";
inline constexpr char kIsInstrumentedBuildKey[] = "is-instrumented-build";
inline constexpr char kNetworkConnectionTypeMetadataKey[] =
    "network-connection-type";
inline constexpr char kNetworkMaxEffectiveConnectionTypeMetadataKey[] =
    "network-max-effective-connection-type";
inline constexpr char kNetworkMinEffectiveConnectionTypeMetadataKey[] =
    "network-min-effective-connection-type";
inline constexpr char kOSArchMetadataKey[] = "os-arch";
inline constexpr char kOSBuildFingerprintMetadataKey[] = "os-build-fingerprint";
inline constexpr char kOSKernelVersionMetadataKey[] = "os-kernel-version";
inline constexpr char kOSNameMetadataKey[] = "os-name";
inline constexpr char kOSVersionMetadataKey[] = "os-version";
inline constexpr char kPackageNameMetadataKey[] = "package-name";
inline constexpr char kPhysicalMemoryMetadataKey[] = "physical-memory";
inline constexpr char kProcessOsArchMetadataKey[] = "process-os-arch";
inline constexpr char kProductVersionMetadataKey[] = "product-version";

void AddMetadataToBundle(const metrics::SystemProfileProto& system_profile,
                         perfetto::protos::pbzero::ChromeEventBundle* bundle) {
  MetadataDataSource::AddMetadataToBundle(kOSNameMetadataKey,
                                          system_profile.os().name(), bundle);
  MetadataDataSource::AddMetadataToBundle(
      kOSVersionMetadataKey, system_profile.os().version(), bundle);
  MetadataDataSource::AddMetadataToBundle(kOSKernelVersionMetadataKey,
                                          system_profile.os().kernel_version(),
                                          bundle);
  MetadataDataSource::AddMetadataToBundle(
      kOSBuildFingerprintMetadataKey, system_profile.os().build_fingerprint(),
      bundle);

  MetadataDataSource::AddMetadataToBundle(kProductVersionMetadataKey,
                                          system_profile.app_version(), bundle);
  MetadataDataSource::AddMetadataToBundle(
      kChannelMetadataKey,
      metrics::SystemProfileProto::Channel_Name(system_profile.channel()),
      bundle);
  MetadataDataSource::AddMetadataToBundle(
      kAppLocaleMetadataKey, system_profile.application_locale(), bundle);
  if (system_profile.has_app_package_name()) {
    MetadataDataSource::AddMetadataToBundle(
        kPackageNameMetadataKey, system_profile.app_package_name(), bundle);
  }

  MetadataDataSource::AddMetadataToBundle(
      kOSArchMetadataKey, system_profile.hardware().cpu_architecture(), bundle);
  MetadataDataSource::AddMetadataToBundle(
      kProcessOsArchMetadataKey,
      system_profile.hardware().app_cpu_architecture(), bundle);
  MetadataDataSource::AddMetadataToBundle(
      kPhysicalMemoryMetadataKey,
      base::NumberToString(system_profile.hardware().system_ram_mb()), bundle);
  MetadataDataSource::AddMetadataToBundle(
      kHardwareClassMetadataKey, system_profile.hardware().hardware_class(),
      bundle);
  MetadataDataSource::AddMetadataToBundle(
      kFullHardwareClassMetadataKey,
      system_profile.hardware().full_hardware_class(), bundle);
  MetadataDataSource::AddMetadataToBundle(
      kCpuCoresMetadataKey,
      base::Value(
          static_cast<int>(system_profile.hardware().cpu().num_cores())),
      bundle);
  MetadataDataSource::AddMetadataToBundle(
      kCpuEfficientCoresMetadataKey,
      base::Value(static_cast<int>(
          system_profile.hardware().cpu().num_efficient_cores())),
      bundle);
  MetadataDataSource::AddMetadataToBundle(
      kCpuIsHypervisorMetadataKey,
      base::Value(system_profile.hardware().cpu().is_hypervisor()), bundle);
  MetadataDataSource::AddMetadataToBundle(
      kCpuVenderMetadataKey, system_profile.hardware().cpu().vendor_name(),
      bundle);
  MetadataDataSource::AddMetadataToBundle(
      kCpuSignatureMetadataKey,
      base::NumberToString(system_profile.hardware().cpu().signature()),
      bundle);

  MetadataDataSource::AddMetadataToBundle(
      kGpuVendorIdMetadataKey,
      base::NumberToString(system_profile.hardware().gpu().vendor_id()),
      bundle);
  MetadataDataSource::AddMetadataToBundle(
      kGpuDriverVersionMetadataKey,
      system_profile.hardware().gpu().driver_version(), bundle);
  MetadataDataSource::AddMetadataToBundle(
      kGpuGlVendorMetadataKey, system_profile.hardware().gpu().gl_vendor(),
      bundle);
  MetadataDataSource::AddMetadataToBundle(
      kGpuGlRendererMetadataKey, system_profile.hardware().gpu().gl_renderer(),
      bundle);

  const bool has_seek_penalty =
      system_profile.hardware().app_drive().has_seek_penalty() ||
      system_profile.hardware().user_data_drive().has_seek_penalty();
  MetadataDataSource::AddMetadataToBundle(
      kDriveHasSeekPenaltyKey, base::Value(has_seek_penalty), bundle);

  MetadataDataSource::AddMetadataToBundle(
      kIsInstrumentedBuildKey,
      base::Value(system_profile.is_instrumented_build()), bundle);

  for (const auto& accessibility_state :
       system_profile.accessibility_state().enabled_modes()) {
    MetadataDataSource::AddMetadataToBundle(
        kAccessibilityEnabledModesMetadataKey,
        metrics::SystemProfileProto::AccessibilityState::AXMode_Name(
            accessibility_state),
        bundle);
  }

  MetadataDataSource::AddMetadataToBundle(
      kNetworkConnectionTypeMetadataKey,
      metrics::SystemProfileProto::Network::ConnectionType_Name(
          system_profile.network().connection_type()),
      bundle);
  MetadataDataSource::AddMetadataToBundle(
      kNetworkMinEffectiveConnectionTypeMetadataKey,
      metrics::SystemProfileProto::Network::EffectiveConnectionType_Name(
          system_profile.network().min_effective_connection_type()),
      bundle);
  MetadataDataSource::AddMetadataToBundle(
      kNetworkMaxEffectiveConnectionTypeMetadataKey,
      metrics::SystemProfileProto::Network::EffectiveConnectionType_Name(
          system_profile.network().max_effective_connection_type()),
      bundle);

  for (const metrics::SystemProfileProto::AntiVirusProduct& antivirus_product :
       system_profile.antivirus_product()) {
    MetadataDataSource::AddMetadataToBundle(
        base::StrCat({kAntivirusProductMetadataPrefix,
                      antivirus_product.product_name()}),
        antivirus_product.has_product_state()
            ? metrics::SystemProfileProto::AntiVirusState_Name(
                  antivirus_product.product_state())
            : "",
        bundle);
  }
}

}  // namespace

void RecordSystemProfileMetadata(
    perfetto::protos::pbzero::ChromeEventBundle* bundle) {
  metrics::SystemProfileProto system_profile;
  auto recorder =
      BackgroundTracingMetricsProvider::GetSystemProfileMetricsRecorder();
  if (!recorder) {
    return MetadataDataSource::RecordDefaultBundleMetadata(bundle);
  }
  recorder.Run(system_profile);
  AddMetadataToBundle(system_profile, bundle);
}

}  // namespace tracing
