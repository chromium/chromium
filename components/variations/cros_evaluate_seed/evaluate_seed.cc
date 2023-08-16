// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros_evaluate_seed/evaluate_seed.h"

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "chromeos/ash/components/dbus/featured/featured.pb.h"
#include "chromeos/crosapi/cpp/channel_to_enum.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/service/variations_field_trial_creator.h"

namespace variations::cros_early_boot::evaluate_seed {

namespace {
constexpr char kSafeSeedSwitch[] = "use-safe-seed";
constexpr char kEnterpriseEnrolledSwitch[] = "enterprise-enrolled";
}  // namespace

base::Version CrosVariationsServiceClient::GetVersionForSimulation() {
  // TODO(mutexlox): Get the version that will be used on restart instead of
  // the current version IF this is necessary. (We may not need simulations for
  // early-boot experiments.)
  // See ChromeVariationsServiceClient::GetVersionForSimulation.
  return version_info::GetVersion();
}

scoped_refptr<network::SharedURLLoaderFactory>
CrosVariationsServiceClient::GetURLLoaderFactory() {
  // Do not load any data on CrOS early boot. This function is only called to
  // fetch a new seed, and we should not fetch new seeds in evaluate_seed.
  return nullptr;
}

network_time::NetworkTimeTracker*
CrosVariationsServiceClient::GetNetworkTimeTracker() {
  // Do not load any data on CrOS early boot; evaluate_seed should not load new
  // seeds.
  return nullptr;
}

bool CrosVariationsServiceClient::OverridesRestrictParameter(
    std::string* parameter) {
  // TODO(b/263975722): Implement.
  return false;
}

// Get the active channel, if applicable.
version_info::Channel CrosVariationsServiceClient::GetChannel() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::string channel;
  if (base::SysInfo::GetLsbReleaseValue(crosapi::kChromeOSReleaseTrack,
                                        &channel)) {
    return crosapi::ChannelToEnum(channel);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return version_info::Channel::UNKNOWN;
}

bool CrosVariationsServiceClient::IsEnterprise() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnterpriseEnrolledSwitch);
}

std::unique_ptr<ClientFilterableState> GetClientFilterableState() {
  CrosVariationsServiceClient client;

  // TODO(b/263975722): Properly use VariationsServiceClient and
  // VariationsFieldTrialCreator::GetClientFilterableStateForVersion.
  auto state = std::make_unique<ClientFilterableState>(
      base::BindOnce([](bool enrolled) { return enrolled; },
                     client.IsEnterprise()),
      base::BindOnce([] { return base::flat_set<uint64_t>(); }));

  state->channel =
      ConvertProductChannelToStudyChannel(client.GetChannelForVariations());
  state->form_factor = client.GetCurrentFormFactor();

  return state;
}

absl::optional<SafeSeed> GetSafeSeedData(FILE* stream) {
  featured::SeedDetails safe_seed;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kSafeSeedSwitch)) {
    // Read safe seed from |stream|.
    std::string safe_seed_data;
    if (!base::ReadStreamToString(stream, &safe_seed_data)) {
      PLOG(ERROR) << "Failed to read from stream:";
      return absl::nullopt;
    }
    // Parse safe seed.
    if (!safe_seed.ParseFromString(safe_seed_data)) {
      LOG(ERROR) << "Failed to parse proto from input";
      return absl::nullopt;
    }
    return SafeSeed{true, safe_seed};
  }
  return SafeSeed{false, safe_seed};
}

int EvaluateSeedMain(FILE* stream) {
  absl::optional<SafeSeed> safe_seed = GetSafeSeedData(stream);
  if (!safe_seed.has_value()) {
    LOG(ERROR) << "Failed to read seed from stdin";
    return EXIT_FAILURE;
  }

  std::unique_ptr<ClientFilterableState> state = GetClientFilterableState();

  // TODO(b/263975722): Implement this binary.
  (void)state;

  return EXIT_SUCCESS;
}

}  // namespace variations::cros_early_boot::evaluate_seed
