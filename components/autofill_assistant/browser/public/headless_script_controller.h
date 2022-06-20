// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_HEADLESS_SCRIPT_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_HEADLESS_SCRIPT_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/public/external_action.pb.h"
#include "components/autofill_assistant/browser/public/runtime_observer.h"
#include "components/autofill_assistant/browser/public/ui_state.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

// Allows to execute AutofillAssistant scripts.
class HeadlessScriptController {
 public:
  struct ScriptResult {
    // TODO(b/209429727): use canonical status codes instead.
    bool success = false;
  };

  // Fetches and executes the script specified by |script_parameters|.
  // At most one script can be executed at the same time, if a script is already
  // being executed at the time of this call it will return an error.
  // If this instance of |HeadlessScriptController| is destroyed the script
  // execution will be interrupted.
  virtual void StartScript(
      const base::flat_map<std::string, std::string>& script_parameters,
      base::OnceCallback<void(ScriptResult)> script_ended_callback) = 0;

  virtual ~HeadlessScriptController() = default;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_HEADLESS_SCRIPT_CONTROLLER_H_
