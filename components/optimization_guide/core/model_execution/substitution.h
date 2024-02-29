// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SUBSTITUTION_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SUBSTITUTION_H_

#include <optional>
#include <string>
#include <vector>

#include "components/optimization_guide/proto/substitution.pb.h"

namespace optimization_guide {

struct SubstitutionResult {
  std::string input_string;
  bool should_ignore_input_context;
};

std::optional<SubstitutionResult> CreateSubstitutions(
    const google::protobuf::MessageLite& request,
    const google::protobuf::RepeatedPtrField<proto::SubstitutedString>&
        config_substitutions);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_SUBSTITUTION_H_
