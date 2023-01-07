// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_RUNTIME_OBSERVER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_RUNTIME_OBSERVER_H_

#include "base/observer_list_types.h"
#include "components/autofill_assistant/browser/public/ui_state.h"

namespace autofill_assistant {
// Observes RuntimeManager's states.
class RuntimeObserver : public base::CheckedObserver {
 public:
  // Called when the assistant state is changed.
  virtual void OnStateChanged(UIState state) = 0;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_RUNTIME_OBSERVER_H_
