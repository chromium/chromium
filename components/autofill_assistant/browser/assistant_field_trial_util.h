// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_FIELD_TRIAL_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_FIELD_TRIAL_UTIL_H_

#include "base/strings/string_piece.h"

namespace autofill_assistant {

class AssistantFieldTrialUtil {
 public:
  virtual ~AssistantFieldTrialUtil() = default;

  virtual bool RegisterSyntheticFieldTrial(
      base::StringPiece trial_name,
      base::StringPiece group_name) const = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_FIELD_TRIAL_UTIL_H_
