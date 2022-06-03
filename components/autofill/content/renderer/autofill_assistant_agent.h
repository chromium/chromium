// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_ASSISTANT_AGENT_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_ASSISTANT_AGENT_H_

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"

namespace autofill {

// This class stores the state of Autofill Assistant.
class AutofillAssistantAgent : public content::RenderFrameObserver {
 public:
  explicit AutofillAssistantAgent(content::RenderFrame* render_frame);
  ~AutofillAssistantAgent() override;

  AutofillAssistantAgent(const AutofillAssistantAgent&) = delete;
  AutofillAssistantAgent& operator=(const AutofillAssistantAgent&) = delete;

  // Returns whether the keyboard should be suppressed at this time.
  bool ShouldSuppressKeyboard() const;

  void EnableKeyboard();

  // Disables the keyboard. This can disable the keyboard until the tab is
  // closed. It should always be called with an RAII object to ensure that the
  // state is reset after use.
  void DisableKeyboard();

  // RenderFrameObserver:
  void OnDestruct() override;

 private:
  bool should_suppress_keyboard_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_AUTOFILL_ASSISTANT_AGENT_H_
