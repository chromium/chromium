// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/at_memory/logging_util.h"

#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/personal_context/proto/features/at_memory.pb.h"

namespace autofill {

using ::personal_context::proto::AutofillFetchPlan;

LogBuffer& operator<<(LogBuffer& buffer, const AutofillFetchPlan& plan) {
  // TODO(crbug.com/547641054): Log full fetch plan details.
  return buffer;
}

}  // namespace autofill
