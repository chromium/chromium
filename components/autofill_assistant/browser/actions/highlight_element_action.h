// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_HIGHLIGHT_ELEMENT_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_HIGHLIGHT_ELEMENT_ACTION_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// An action to highlight an element on Web.
// TODO(crbug.com/806868): This action should be more configurable instead of
// using hardcoded css styling since it depends on the content of a page.
class HighlightElementAction : public Action {
 public:
  explicit HighlightElementAction(ActionDelegate* delegate,
                                  const ActionProto& proto);
  ~HighlightElementAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnWaitForElement(ProcessActionCallback callback,
                        const Selector& selector,
                        const ClientStatus& element_status);
  void OnHighlightElement(ProcessActionCallback callback,
                          const ClientStatus& status);

  base::WeakPtrFactory<HighlightElementAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HighlightElementAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_HIGHLIGHT_ELEMENT_ACTION_H_
