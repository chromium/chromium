// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros/evaluate_seed.h"

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "chromeos/crosapi/cpp/channel_to_enum.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/service/variations_field_trial_creator.h"

namespace variations::evaluate_seed {

namespace {
constexpr char kSafeSeedSwitch[] = "use-safe-seed";
constexpr char kEnterpriseEnrolledSwitch[] = "enterprise-enrolled";
const char kFakeVariationsChannel[] = "fake-variations-channel";
}  // namespace

// Get the active channel, if applicable.
Study::Channel GetChannel(const base::CommandLine* command_line) {
  std::string channel;
  if (command_line->HasSwitch(kFakeVariationsChannel)) {
    channel = command_line->GetSwitchValueASCII(kFakeVariationsChannel);
  }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  else if (base::SysInfo::GetLsbReleaseValue(crosapi::kChromeOSReleaseTrack,
                                             &channel)) {
    // do nothing; we successfully got channel.
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  else {
    // We didn't get the channel.
    return Study::UNKNOWN;
  }

  return ConvertProductChannelToStudyChannel(crosapi::ChannelToEnum(channel));
}

std::unique_ptr<ClientFilterableState> GetClientFilterableState(
    const base::CommandLine* command_line) {
  bool enterprise_enrolled = command_line->HasSwitch(kEnterpriseEnrolledSwitch);

  // TODO(b/263975722): Fill in the rest of ClientFilterableState.
  auto state = std::make_unique<ClientFilterableState>(base::BindOnce(
      [](bool enrolled) { return enrolled; }, enterprise_enrolled));

  state->channel = GetChannel(command_line);
  return state;
}

absl::optional<SafeSeed> GetSafeSeedData(const base::CommandLine* command_line,
                                         FILE* stream) {
  if (command_line->HasSwitch(kSafeSeedSwitch)) {
    // Read safe seed from |stream|.
    std::string safe_seed_data;
    if (!base::ReadStreamToString(stream, &safe_seed_data)) {
      PLOG(ERROR) << "Failed to read from stream:";
      return absl::nullopt;
    }
    return SafeSeed{true, safe_seed_data};
  }
  return SafeSeed{false, ""};
}

int EvaluateSeedMain(const base::CommandLine* command_line, FILE* stream) {
  absl::optional<SafeSeed> safe_seed = GetSafeSeedData(command_line, stream);
  if (!safe_seed.has_value()) {
    LOG(ERROR) << "Failed to read seed from stdin";
    return EXIT_FAILURE;
  }

  std::unique_ptr<ClientFilterableState> state =
      GetClientFilterableState(command_line);

  // TODO(b/263975722): Implement this binary.
  (void)state;

  return EXIT_SUCCESS;
}

}  // namespace variations::evaluate_seed
