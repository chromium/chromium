// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stability_report/user_stream_data_source.h"

namespace stability_report {

ProcessState& AddProcessForSnapshot(const base::ProcessId process_id,
                                    StabilityReport* report) {
#if DCHECK_IS_ON()
  // Ensure no ProcessState was created yet for the process in question.
  for (const ProcessState& process_state : report->process_states()) {
    DCHECK_NE(process_state.process_id(), process_id);
  }
#endif

  ProcessState* process_state = report->add_process_states();
  process_state->set_process_id(process_id);
  return *process_state;
}
}  // namespace stability_report
