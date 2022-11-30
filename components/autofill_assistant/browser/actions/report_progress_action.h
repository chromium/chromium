// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REPORT_PROGRESS_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REPORT_PROGRESS_ACTION_H_

#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// An action to report script progress through a flow.
class ReportProgressAction : public Action {
 public:
  explicit ReportProgressAction(ActionDelegate* delegate,
                                const ActionProto& proto);

  ReportProgressAction(const ReportProgressAction&) = delete;
  ReportProgressAction& operator=(const ReportProgressAction&) = delete;

  ~ReportProgressAction() override;

 private:
  void InternalProcessAction(ProcessActionCallback callback) override;
  void OnReportProgress(bool success);

  base::WeakPtrFactory<ReportProgressAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_REPORT_PROGRESS_ACTION_H_
