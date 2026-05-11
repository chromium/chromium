// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_DEFINITION_PARSING_UTILS_H_
#define COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_DEFINITION_PARSING_UTILS_H_

#include <string>

#include "base/types/expected.h"
#include "base/values.h"
#include "components/record_replay/core/browser/recording.pb.h"

namespace record_replay {

// Parses a single task definition from a JSON dictionary.
// Returns the parsed TaskDefinition if valid, or an error string on
// failure.
base::expected<TaskDefinition, std::string> ParseTaskDefinition(
    const base::DictValue& dict);

}  // namespace record_replay

#endif  // COMPONENTS_RECORD_REPLAY_CORE_BROWSER_TASK_DEFINITION_PARSING_UTILS_H_
