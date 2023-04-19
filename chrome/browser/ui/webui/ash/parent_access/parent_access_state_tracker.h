// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_STATE_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_STATE_TRACKER_H_

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"

namespace ash {

class ParentAccessStateTracker {
 public:
  // The flow state used to record the status of the flow when it is closed. The
  // enum values are recorded in histogram therefore, keep it consistent with
  // ParentAccessFlowResult enum in enums.xml.
  enum class FlowResult : int {
    // User has opened the initial screen of the flow, if it exists.
    // TOOD(b/258722110): Add implementation for recording the initial to
    // authentication screen transition.
    kInitial = 0,
    // User has reached the authentication screen.
    kParentAuthentication = 1,
    // User has reached the approval screen.
    kApproval = 2,
    // State when parent grants access.
    kAccessApproved = 3,
    // State when parent declines access.
    kAccessDeclined = 4,
    // State where the error page is shown.
    kError = 5,
    // State where parent has disabled permission requests.
    kRequestsDisabled = 6,
    kNumStates = 7
  };

  // `flow_type` indicates which Parent Access flow type is being shown.
  // `is_disabled` indicates if requests have been disabled by a parent. These
  // parameters are used to determine the initial state of the flow.
  explicit ParentAccessStateTracker(
      parent_access_ui::mojom::ParentAccessParams::FlowType flow_type,
      bool is_disabled);
  ParentAccessStateTracker(const ParentAccessStateTracker&) = delete;
  ParentAccessStateTracker& operator=(const ParentAccessStateTracker&) = delete;
  ~ParentAccessStateTracker();

  void OnWebUiStateChanged(FlowResult result);

 private:
  FlowResult flow_result_;

  const parent_access_ui::mojom::ParentAccessParams::FlowType flow_type_;
};
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_STATE_TRACKER_H_
