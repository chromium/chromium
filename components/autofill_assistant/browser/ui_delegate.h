// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_

#include <string>

namespace autofill_assistant {
// UI delegate called for script executions.
class UiDelegate {
 public:
  virtual ~UiDelegate() = default;

  // Called when the overlay has been clicked.
  virtual void OnClickOverlay() = 0;

  // Called when the Autofill Assistant should be destroyed, e.g. the tab
  // detached from the associated activity.
  virtual void OnDestroy() = 0;

  // Called when a script was selected for execution.
  virtual void OnScriptSelected(const std::string& script_path) = 0;

 protected:
  UiDelegate() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_
