// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_progress_bar_action.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/numerics/ranges.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

ShowProgressBarAction::ShowProgressBarAction(ActionDelegate* delegate,
                                             const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_show_progress_bar());
}

ShowProgressBarAction::~ShowProgressBarAction() {}

void ShowProgressBarAction::InternalProcessAction(
    ProcessActionCallback callback) {
  if (proto_.show_progress_bar().has_message()) {
    // TODO(crbug.com/806868): Deprecate and remove message from this action and
    // use tell instead.
    delegate_->SetStatusMessage(proto_.show_progress_bar().message());
  }
  int progress =
      base::ClampToRange(proto_.show_progress_bar().progress(), 0, 100);
  delegate_->SetProgress(progress);
  if (proto_.show_progress_bar().has_hide()) {
    delegate_->SetProgressVisible(!proto_.show_progress_bar().hide());
  }

  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
