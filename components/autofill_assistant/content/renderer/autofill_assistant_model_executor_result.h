// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_MODEL_EXECUTOR_RESULT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_MODEL_EXECUTOR_RESULT_H_

namespace autofill_assistant {

// Result used to communicated model execution results between interested
// parties.
struct ModelExecutorResult {
  ModelExecutorResult() = default;
  ModelExecutorResult(int r, int o, bool with_override)
      : role(r), objective(o), used_override(with_override) {}

  // Role and objective pair.
  int role = 0;
  int objective = 0;
  // Whether this result came from an override.
  bool used_override = false;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_MODEL_EXECUTOR_RESULT_H_
