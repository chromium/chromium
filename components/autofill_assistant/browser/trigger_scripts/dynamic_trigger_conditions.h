// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_DYNAMIC_TRIGGER_CONDITIONS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_DYNAMIC_TRIGGER_CONDITIONS_H_

#include "base/optional.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

namespace autofill_assistant {

// Provides easy access to the values of dynamic trigger conditions. Dynamic
// trigger conditions depend on the current state of the DOM tree and need to be
// repeatedly re-evaluated.
class DynamicTriggerConditions {
 public:
  DynamicTriggerConditions();
  ~DynamicTriggerConditions();

  // TODO(b/171776026): implement this stub.
  virtual base::Optional<bool> GetSelectorMatches(
      const Selector& selector) const;

  virtual void Update(WebController* web_controller,
                      base::OnceCallback<void(void)> callback);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_DYNAMIC_TRIGGER_CONDITIONS_H_
