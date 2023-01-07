// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PROMPT_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PROMPT_ACTION_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/chip.h"
#include "components/autofill_assistant/browser/user_action.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// Allow the selection of one or more suggestions.
class PromptAction : public Action {
 public:
  explicit PromptAction(ActionDelegate* delegate, const ActionProto& proto);

  PromptAction(const PromptAction&) = delete;
  PromptAction& operator=(const PromptAction&) = delete;

  ~PromptAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void RegisterChecks(
      BatchElementChecker* checker,
      base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback);
  void SetupConditions();
  bool HasNonemptyPreconditions();
  void OnPreconditionResult(
      size_t choice_index,
      const ClientStatus& status,
      const std::vector<std::string>& ignored_payloads,
      const std::vector<std::string>& ignored_tags,
      const base::flat_map<std::string, DomObjectFrameStack>& ignored_elements);
  void UpdateUserActions();
  void OnAutoSelectCondition(
      const ClientStatus& status,
      const std::vector<std::string>& payloads,
      const std::vector<std::string>& tags,
      const base::flat_map<std::string, DomObjectFrameStack>& ignored_elements);
  void OnElementChecksDone(
      base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback);
  void OnDoneWaitForDom(const ClientStatus& status);
  void OnSuggestionChosen(int choice_index);
  void OnNavigationEnded();
  void EndAction(const ClientStatus& status);
  void UpdateTimings();

  ProcessActionCallback callback_;

  // preconditions_[i] contains the element preconditions for
  // proto.prompt.choice[i].
  std::vector<ElementConditionProto> preconditions_;

  // precondition_results_[i] contains the last result reported by
  // preconditions_[i].
  std::vector<bool> precondition_results_;
  // positive_precondition_changes_[i] contains true only when the corresponding
  // preconditions_[i] changed from false to true in the last periodic checks.
  std::vector<bool> positive_precondition_changes_;
  // precondition_stopwatches_[i] contains a stopwatch with the active time for
  // preconditions_[i]. This will be 0 as long as preconditions_[i] is false
  // and will contain the sum of the time that the precondition checks required
  // to complete plus half the duration of the retry period.
  std::vector<Stopwatch> precondition_stopwatches_;

  // true if something in precondition_results_ has changed, which means that
  // the set of user actions must be updated.
  bool precondition_changed_ = false;

  // The action ends once this precondition matches. The payload points
  // to the specific choice that matched.
  absl::optional<ElementConditionProto> auto_select_;

  // If >= 0, contains the index of the Choice to auto-select. Set based on the
  // payload reported by |auto_select_|.
  int auto_select_choice_index_ = -1;

  // Batch element checker for preconditions and auto-selection.
  std::unique_ptr<BatchElementChecker> element_checker_;

  // This stopwatch contains the total wait time, needed all exit criteria
  // except the autoselect one.
  Stopwatch wait_time_stopwatch_;
  // Contains the duration of the last retry period.
  Stopwatch last_period_stopwatch_;
  // Contains the duration of the last precondition checks.
  Stopwatch last_checks_stopwatch_;

  base::WeakPtrFactory<PromptAction> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PROMPT_ACTION_H_
