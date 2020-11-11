// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_CAST_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_CAST_ACTION_H_

#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/top_padding.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace autofill_assistant {

// An action to show cast a given element on Web. Scrolling to it first if
// required.
class ShowCastAction : public Action {
 public:
  explicit ShowCastAction(ActionDelegate* delegate, const ActionProto& proto);
  ~ShowCastAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void OnWaitForElement(const Selector& selector,
                        const TopPadding& top_padding,
                        const ClientStatus& element_status);
  void OnScrollToElementPosition(const ClientStatus& status);

  void EndAction(const ClientStatus& status);

  ProcessActionCallback process_action_callback_;

  base::WeakPtrFactory<ShowCastAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ShowCastAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_SHOW_CAST_ACTION_H_
