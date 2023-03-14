// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_H_
#define COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_H_

#include <memory>
#include <string>

#include <stdio.h>

#include "base/command_line.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/proto/cros_safe_seed.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace variations::evaluate_seed {

// Retrieve a ClientFilterableState struct based on the given |command_line|.
std::unique_ptr<ClientFilterableState> GetClientFilterableState(
    const base::CommandLine* command_line);

struct SafeSeed {
  bool use_safe_seed = false;
  variations::SeedDetails seed_data;
};

// Read the safe seed data from |stream|, if and only if the |command_line|
// indicates that we should use the safe seed.
absl::optional<SafeSeed> GetSafeSeedData(const base::CommandLine* command_line,
                                         FILE* stream);

// Evaluate the seed state, writing serialized computed output to stdout.
// Reads a proto from |stream| for data like safe seed.
// Return values are standard for main methods (EXIT_SUCCESS / EXIT_FAILURE).
int EvaluateSeedMain(const base::CommandLine* command_line, FILE* stream);

}  // namespace variations::evaluate_seed

#endif  // COMPONENTS_VARIATIONS_CROS_EVALUATE_SEED_H_
