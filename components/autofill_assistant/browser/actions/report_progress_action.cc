// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/report_progress_action.h"

#include "base/callback.h"
#include "base/metrics/field_trial.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

namespace {

// When starting a report progress action, a synthetic field trial is recorded.
// This is used to allow tracking stability metrics as we start using this new
// action. Note there is no control group - this is purely for stability
// tracking.
const char kReportProgressSyntheticFieldTrialName[] =
    "AutofillAssistantReportProgressAction";
const char kReportProgressEnabledGroup[] = "Enabled";

}  // namespace

ReportProgressAction::ReportProgressAction(ActionDelegate* delegate,
                                           const ActionProto& proto)
    : Action(delegate, proto) {}

ReportProgressAction::~ReportProgressAction() = default;

void ReportProgressAction::InternalProcessAction(
    ProcessActionCallback callback) {
  base::FieldTrialList::CreateFieldTrial(kReportProgressSyntheticFieldTrialName,
                                         kReportProgressEnabledGroup);
  delegate_->ReportProgress(
      proto_.report_progress().payload(),
      base::BindOnce(&ReportProgressAction::OnReportProgress,
                     weak_ptr_factory_.GetWeakPtr()));
  // The action is done after the call is made; we don't wait for the callback
  // to update it, because we don't care what the response is.
  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback).Run(std::move(processed_action_proto_));
}

// The script continues whether the status is successful or not
// (fire-and-forget). We could later gather metrics based on the response code
// using this callback.
void ReportProgressAction::OnReportProgress(bool success) {}

}  // namespace autofill_assistant
