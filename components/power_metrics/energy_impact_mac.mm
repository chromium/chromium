// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/power_metrics/energy_impact_mac.h"

#include <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/power_metrics/mach_time_mac.h"
#include "components/power_metrics/resource_coalition_mac.h"

namespace power_metrics {

namespace {

NSDictionary* MaybeGetDictionaryFromPath(const base::FilePath& path) {
  // The folder where the energy coefficient plist files are stored.
  return [NSDictionary
      dictionaryWithContentsOfURL:base::apple::FilePathToNSURL(path)
                            error:nil];
}

double GetNamedCoefficientOrZero(NSDictionary* dict, NSString* key) {
  NSNumber* num = base::apple::ObjCCast<NSNumber>(dict[key]);
  return num.floatValue;
}

}  // namespace

std::optional<EnergyImpactCoefficients>
ReadCoefficientsForCurrentMachineOrDefault() {
  std::optional<std::string> board_id = internal::GetBoardIdForThisMachine();
  if (!board_id.has_value())
    return std::nullopt;

  return internal::ReadCoefficientsForBoardIdOrDefault(
      base::FilePath(FILE_PATH_LITERAL("/usr/share/pmenergy")),
      board_id.value());
}

double ComputeEnergyImpactForResourceUsage(
    const coalition_resource_usage& data_sample,
    const EnergyImpactCoefficients& coefficients,
    const mach_timebase_info_data_t& mach_timebase) {
  // TODO(crbug.com/40197639): The below coefficients are not used
  // for now. Their units are unknown, and in the case of the network-related
  // coefficients, it's not clear how to sample the data.
  //
  // coefficients.kdiskio_bytesread;
  // coefficients.kdiskio_byteswritten;
  // coefficients.knetwork_recv_bytes;
  // coefficients.knetwork_recv_packets;
  // coefficients.knetwork_sent_bytes;
  // coefficients.knetwork_sent_packets;

  // The cumulative CPU usage in |data_sample| is in units of ns, and
  // |cpu_time_equivalent_ns| is computed in CPU ns up to the end of this
  // function, where it's converted to units of EnergyImpact.
  double cpu_time_equivalent_ns = 0.0;

  // The kcpu_wakeups coefficient on disk is in seconds, but our
  // intermediate result is in ns, so convert to ns on the fly.
  cpu_time_equivalent_ns += coefficients.kcpu_wakeups *
                            base::Time::kNanosecondsPerSecond *
                            data_sample.platform_idle_wakeups;

  // Presumably the kgpu_time coefficient has suitable units for the
  // conversion of GPU time energy to CPU time energy. There is a fairly
  // wide spread on this constant seen in /usr/share/pmenergy. On
  // macOS 11.5.2 the spread is from 0 through 5.9.
  cpu_time_equivalent_ns += coefficients.kgpu_time *
                            MachTimeToNs(data_sample.gpu_time, mach_timebase);

  cpu_time_equivalent_ns +=
      coefficients.kqos_background *
      MachTimeToNs(data_sample.cpu_time_eqos[THREAD_QOS_BACKGROUND],
                   mach_timebase);
  cpu_time_equivalent_ns +=
      coefficients.kqos_default *
      MachTimeToNs(data_sample.cpu_time_eqos[THREAD_QOS_DEFAULT],
                   mach_timebase);
  cpu_time_equivalent_ns +=
      coefficients.kqos_legacy *
      MachTimeToNs(data_sample.cpu_time_eqos[THREAD_QOS_LEGACY], mach_timebase);
  cpu_time_equivalent_ns +=
      coefficients.kqos_user_initiated *
      MachTimeToNs(data_sample.cpu_time_eqos[THREAD_QOS_USER_INITIATED],
                   mach_timebase);
  cpu_time_equivalent_ns +=
      coefficients.kqos_user_interactive *
      MachTimeToNs(data_sample.cpu_time_eqos[THREAD_QOS_USER_INTERACTIVE],
                   mach_timebase);
  cpu_time_equivalent_ns +=
      coefficients.kqos_utility *
      MachTimeToNs(data_sample.cpu_time_eqos[THREAD_QOS_UTILITY],
                   mach_timebase);

  // The conversion ratio for CPU time/EnergyImpact is ns/10ms
  constexpr double kNsToEI = 1E-7;
  return cpu_time_equivalent_ns * kNsToEI;
}

namespace internal {

std::optional<EnergyImpactCoefficients> ReadCoefficientsFromPath(
    const base::FilePath& plist_file) {
  @autoreleasepool {
    NSDictionary* dict = MaybeGetDictionaryFromPath(plist_file);
    if (!dict) {
      return std::nullopt;
    }

    NSDictionary* energy_constants = dict[@"energy_constants"];
    if (!energy_constants) {
      return std::nullopt;
    }

    EnergyImpactCoefficients coefficients{};
    coefficients.kcpu_time =
        GetNamedCoefficientOrZero(energy_constants, @"kcpu_time");
    coefficients.kcpu_wakeups =
        GetNamedCoefficientOrZero(energy_constants, @"kcpu_wakeups");

    coefficients.kqos_default =
        GetNamedCoefficientOrZero(energy_constants, @"kqos_default");
    coefficients.kqos_background =
        GetNamedCoefficientOrZero(energy_constants, @"kqos_background");
    coefficients.kqos_utility =
        GetNamedCoefficientOrZero(energy_constants, @"kqos_utility");
    coefficients.kqos_legacy =
        GetNamedCoefficientOrZero(energy_constants, @"kqos_legacy");
    coefficients.kqos_user_initiated =
        GetNamedCoefficientOrZero(energy_constants, @"kqos_user_initiated");
    coefficients.kqos_user_interactive =
        GetNamedCoefficientOrZero(energy_constants, @"kqos_user_interactive");

    coefficients.kdiskio_bytesread =
        GetNamedCoefficientOrZero(energy_constants, @"kdiskio_bytesread");
    coefficients.kdiskio_byteswritten =
        GetNamedCoefficientOrZero(energy_constants, @"kdiskio_byteswritten");

    coefficients.kgpu_time =
        GetNamedCoefficientOrZero(energy_constants, @"kgpu_time");

    coefficients.knetwork_recv_bytes =
        GetNamedCoefficientOrZero(energy_constants, @"knetwork_recv_bytes");
    coefficients.knetwork_recv_packets =
        GetNamedCoefficientOrZero(energy_constants, @"knetwork_recv_packets");
    coefficients.knetwork_sent_bytes =
        GetNamedCoefficientOrZero(energy_constants, @"knetwork_sent_bytes");
    coefficients.knetwork_sent_packets =
        GetNamedCoefficientOrZero(energy_constants, @"knetwork_sent_packets");

    return coefficients;
  }
}

std::optional<EnergyImpactCoefficients> ReadCoefficientsForBoardIdOrDefault(
    const base::FilePath& directory,
    const std::string& board_id) {
  auto coefficients = ReadCoefficientsFromPath(
      directory.Append(board_id).AddExtension(FILE_PATH_LITERAL("plist")));
  if (coefficients.has_value())
    return coefficients;

  return ReadCoefficientsFromPath(
      directory.Append(FILE_PATH_LITERAL("default.plist")));
}

std::optional<std::string> GetBoardIdForThisMachine() {
  base::mac::ScopedIOObject<io_service_t> platform_expert(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPlatformExpertDevice")));
  if (!platform_expert)
    return std::nullopt;

  // This is what libpmenergy is observed to do in order to retrieve the correct
  // coefficients file for the local computer.
  base::apple::ScopedCFTypeRef<CFDataRef> board_id_data(
      base::apple::CFCast<CFDataRef>(IORegistryEntryCreateCFProperty(
          platform_expert.get(), CFSTR("board-id"), kCFAllocatorDefault, 0)));

  if (!board_id_data)
    return std::nullopt;

  return reinterpret_cast<const char*>(CFDataGetBytePtr(board_id_data.get()));
}

}  // namespace internal

}  // namespace power_metrics
