// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros/evaluate_seed.h"

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/logging.h"

namespace variations::evaluate_seed {

namespace {
constexpr char kSafeSeedSwitch[] = "use-safe-seed";
constexpr char kEnterpriseEnrolledSwitch[] = "enterprise-enrolled";
}  // namespace

ClientFilterableState GetClientFilterableState(
    const base::CommandLine* command_line) {
  bool enterprise_enrolled = command_line->HasSwitch(kEnterpriseEnrolledSwitch);

  // TODO(b/263975722): Fill in the rest of ClientFilterableState.
  return ClientFilterableState(base::BindOnce(
      [](bool enrolled) { return enrolled; }, enterprise_enrolled));
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

  ClientFilterableState state = GetClientFilterableState(command_line);

  // TODO(b/263975722): Implement this binary.
  (void)state;

  return EXIT_SUCCESS;
}

}  // namespace variations::evaluate_seed
