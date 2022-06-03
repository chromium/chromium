// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/autofill_assistant_agent.h"

#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"

namespace autofill {

AutofillAssistantAgent::AutofillAssistantAgent(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

AutofillAssistantAgent::~AutofillAssistantAgent() = default;

bool AutofillAssistantAgent::ShouldSuppressKeyboard() const {
  return should_suppress_keyboard_;
}

void AutofillAssistantAgent::EnableKeyboard() {
  should_suppress_keyboard_ = false;
}

void AutofillAssistantAgent::DisableKeyboard() {
  should_suppress_keyboard_ = true;
}

void AutofillAssistantAgent::OnDestruct() {
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

}  // namespace autofill
