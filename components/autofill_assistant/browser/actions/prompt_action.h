// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PROMPT_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PROMPT_ACTION_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/chip.h"
#include "components/autofill_assistant/browser/element_precondition.h"
#include "components/autofill_assistant/browser/user_action.h"

namespace autofill_assistant {

// Allow the selection of one or more suggestions.
class PromptAction : public Action {
 public:
  explicit PromptAction(ActionDelegate* delegate, const ActionProto& proto);
  ~PromptAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void RunPeriodicChecks();
  void SetupPreconditions();
  bool HasNonemptyPreconditions();
  void CheckPreconditions();
  void OnPreconditionResult(size_t choice_index, bool result);
  void OnPreconditionChecksDone();
  void UpdateUserActions();
  bool HasAutoSelect();
  void CheckAutoSelect();
  void OnAutoSelectElementExists(int choice_index,
                                 const ClientStatus& element_status);
  void OnAutoSelectDone();
  void OnSuggestionChosen(int choice_index);

  ProcessActionCallback callback_;

  // preconditions_[i] contains the element preconditions for
  // proto.prompt.choice[i].
  std::vector<std::unique_ptr<ElementPrecondition>> preconditions_;

  // precondition_results_[i] contains the last result reported by
  // preconditions_[i].
  std::vector<bool> precondition_results_;

  // true if something in precondition_results_ has changed, which means that
  // the set of user actions must be updated.
  bool precondition_changed_ = false;

  // Batch element checker for preconditions.
  std::unique_ptr<BatchElementChecker> precondition_checker_;

  // If >= 0, contains the index of the Choice to auto-select.
  int auto_select_choice_index_ = -1;

  // Batch element checker for auto-selection, if any.
  std::unique_ptr<BatchElementChecker> auto_select_checker_;

  std::unique_ptr<base::RepeatingTimer> timer_;

  base::WeakPtrFactory<PromptAction> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PromptAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PROMPT_ACTION_H_
