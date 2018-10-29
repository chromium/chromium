// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_progress_bar_action.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

ShowProgressBarAction::ShowProgressBarAction(const ActionProto& proto)
    : Action(proto) {
  DCHECK(proto_.has_show_progress_bar());
}

ShowProgressBarAction::~ShowProgressBarAction() {}

void ShowProgressBarAction::InternalProcessAction(
    ActionDelegate* delegate,
    ProcessActionCallback callback) {
  int progress =
      std::min(100, std::max(0, proto_.show_progress_bar().progress()));
  if (proto_.show_progress_bar().done() || progress == 100) {
    delegate->HideProgressBar();
  } else {
    delegate->ShowProgressBar(progress, proto_.show_progress_bar().message());
  }

  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
