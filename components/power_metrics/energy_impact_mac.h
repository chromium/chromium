// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_METRICS_ENERGY_IMPACT_MAC_H_
#define COMPONENTS_POWER_METRICS_ENERGY_IMPACT_MAC_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/time/time.h"

struct coalition_resource_usage;

namespace power_metrics {

// Coefficients used to compute the Energy Impact score from resource
// coalition metrics. The order of members mimics the order of keys in the
// plist files in /usr/share/pmenergy.
struct EnergyImpactCoefficients {
  double kcpu_time;

  // In units of seconds/event.
  double kcpu_wakeups;

  // Coefficients for CPU usage at different QOS.
  // Strangely there's no coefficient for maintenance QOS.
  double kqos_default;
  double kqos_background;
  double kqos_utility;
  double kqos_legacy;
  double kqos_user_initiated;
  double kqos_user_interactive;

  double kdiskio_bytesread;
  double kdiskio_byteswritten;

  double kgpu_time;

  double knetwork_recv_bytes;
  double knetwork_recv_packets;
  double knetwork_sent_bytes;
  double knetwork_sent_packets;
};

// Reads the Energy Impact coefficients for the current machine from disk, or
// default coefficients if coefficients are not available for the current
// machine.
std::optional<EnergyImpactCoefficients>
ReadCoefficientsForCurrentMachineOrDefault();

// Computes the Energy Impact score for the resource consumption data in
// |coalition_resource_usage| using |coefficients|.
//
// The Energy Impact (EI) score is referenced to CPU time, such that 10ms CPU
// time appears to be equivalent to 1 EI. The Activity Monitor presents EI
// rates to the user in units of 10ms/s of CPU time. This means a process that
// consumes 1000ms/s or 100% CPU, at default QOS, is rated 100 EI, making the
// two units somewhat relatable. Note that this only has relevance on Intel
// architecture, as it looks like on M1 architecture macOS implements more
// granular, and hopefully more accurate, energy metering on the fly.
double ComputeEnergyImpactForResourceUsage(
    const coalition_resource_usage& coalition_resource_usage,
    const EnergyImpactCoefficients& coefficients,
    const mach_timebase_info_data_t& mach_timebase);

namespace internal {

// Reads the coefficients from the "energy_constants" sub-dictionary of the
// plist file at |plist_file|. This is exposed for testing, production code
// should use ReadCoefficientsForCurrentMachineOrDefault().
std::optional<EnergyImpactCoefficients> ReadCoefficientsFromPath(
    const base::FilePath& plist_file);

// Given a |directory| and a |board_id|, read the plist file for the board id
// from the directory, or if not available, read the default file.
// Returns true if either file can be loaded, false otherwise. This is exposed
// for testing, production code should use
// ReadCoefficientsForCurrentMachineOrDefault().
std::optional<EnergyImpactCoefficients> ReadCoefficientsForBoardIdOrDefault(
    const base::FilePath& directory,
    const std::string& board_id);

// Returns the board id to use for reading the Energy Impact coefficients for
// the current machine. This appears to work for Intel Macs only. This is
// exposed for testing, production code should use
// ReadCoefficientsForCurrentMachineOrDefault().
std::optional<std::string> GetBoardIdForThisMachine();

}  // namespace internal

}  // namespace power_metrics

#endif  // COMPONENTS_POWER_METRICS_ENERGY_IMPACT_MAC_H_
