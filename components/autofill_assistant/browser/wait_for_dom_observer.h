// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WAIT_FOR_DOM_OBSERVER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WAIT_FOR_DOM_OBSERVER_H_

namespace autofill_assistant {

// Observes WaitForDom.
class WaitForDomObserver {
 public:
  // Called before the execution of an interrupt.
  virtual void OnInterruptStarted() = 0;
  // Called before resuming the main script after an interrupt.
  virtual void OnInterruptFinished() = 0;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WAIT_FOR_DOM_OBSERVER_H_
