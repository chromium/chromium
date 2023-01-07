// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BATCH_ELEMENT_CHECKER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BATCH_ELEMENT_CHECKER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "components/autofill_assistant/browser/web/selector_observer.h"

namespace autofill_assistant {
class ElementFinderResult;
class WebController;

// Helper for checking a set of elements at the same time. It avoids duplicate
// checks.
class BatchElementChecker {
 public:
  explicit BatchElementChecker();

  BatchElementChecker(const BatchElementChecker&) = delete;
  BatchElementChecker& operator=(const BatchElementChecker&) = delete;

  virtual ~BatchElementChecker();

  // Callback for AddFieldValueCheck. Argument is true is the element exists.
  // The string contains the field value, or an empty string if accessing the
  // value failed.
  //
  // An ElementCheckCallback must not delete its calling BatchElementChecker.
  using GetFieldValueCallback =
      base::OnceCallback<void(const ClientStatus&, const std::string&)>;

  // Callback for AddElementConditionCheck. Arguments are a client status
  // (Element Resolution Failed if couldn't find element), a vector of payloads,
  // and a map of client_id's to the resulting elements.
  //
  // An ElementConditionCheckCallback must not delete its calling
  // BatchElementChecker.
  using ElementConditionCheckCallback = base::OnceCallback<void(
      const ClientStatus&,
      const std::vector<std::string>& payloads,
      const std::vector<std::string>& tags,
      const base::flat_map<std::string, DomObjectFrameStack>&)>;

  // Returns true if element condition is empty.
  static bool IsElementConditionEmpty(const ElementConditionProto& proto);

  // Checks an element precondition
  //
  // New element precondition checks cannot be added once Run has been called.
  void AddElementConditionCheck(const ElementConditionProto& selector,
                                ElementConditionCheckCallback callback);

  // Gets the value of |selector| and return the result through |callback|. The
  // returned value will be the empty string in case of error or empty value.
  //
  // New field checks cannot be added once Run has been called.
  void AddFieldValueCheck(const Selector& selector,
                          GetFieldValueCallback callback);

  // A callback to call once all the elements have been checked. These callbacks
  // are guaranteed to be called in order.
  //
  // These callback are allowed to delete the current instance.
  void AddAllDoneCallback(base::OnceCallback<void()> all_done);

  // Returns true if all there are no checks to run.
  bool empty() const;

  // Turns on observer mode. When BatchElementChecker runs in observer mode, it
  // waits until any element condition checks or element checks become true.
  void EnableObserver(const SelectorObserver::Settings& settings);

  // Runs the checks. Once all checks are done, calls the callbacks registered
  // to AddAllDoneCallback().
  void Run(WebController* web_controller);

 private:
  // Not using absl::optional because it's hard to see what "if (var)" means.
  struct MatchResult {
    bool checked = false;
    bool match_result = false;
    bool matches() { return checked && match_result; }
  };

  // Results of one independent |Selector| within one |ElementCondition|.
  struct Result {
    Result();
    ~Result();
    Result(const Result&);

    // Selector checked.
    Selector selector;
    // Result of checking that selector.
    MatchResult match{/* checked= */ false, /* match_result= */ false};

    // The identifier given to this result through the script. This identifier
    // can be used to later find the element in the |ElementStore|.
    absl::optional<std::string> client_id;

    // Whether the matching should be done strict or not.
    bool strict = false;

    // Used in SelectorObserver runs, element_id to retrieve result from
    // SelectorObserver.
    int element_id = -1;
  };

  struct ElementConditionCheck {
    ElementConditionCheck();
    ~ElementConditionCheck();
    ElementConditionCheck(ElementConditionCheck&&);

    // Result of individual |Selector|s within the |ElementCondition|.
    std::vector<Result> results;

    // Callback called with the result after the check is done (if observer
    // mode is enabled) or after one condition matches (if observer mode
    // is disabled).
    ElementConditionCheckCallback callback;

    // |ElementConditionProto| to check.
    ElementConditionProto proto;

    // Resulting found elements. Key is the |client_id| in the |Match|
    // |ElementCondition|s (used to refer to this element in the scripts), value
    // is the found element.
    base::flat_map<std::string, DomObjectFrameStack> elements;
  };

  void RunWithObserver(WebController* web_controller);

  // Gets called for each Selector checked.
  void OnSelectorChecked(
      std::vector<std::pair</* element_condition_index */ size_t,
                            /* result_index */ size_t>>* results,
      const ClientStatus& element_status,
      std::unique_ptr<ElementFinderResult> element_result);

  void OnElementPreconditionChecked(
      std::vector<ElementConditionCheckCallback>* callbacks,
      const ClientStatus& element_status);

  // Gets called for each FieldValueCheck.
  void OnFieldValueChecked(std::vector<GetFieldValueCallback>* callbacks,
                           const ClientStatus& status,
                           const std::string& value);

  void CheckDone();
  void FinishedCallbacks();
  void CallAllCallbacksWithError(const ClientStatus& status);

  // Add selectors from |proto| to |results_|, doing a depth-first search.
  void AddElementConditionResults(const ElementConditionProto& proto,
                                  size_t element_condition_index);

  MatchResult EvaluateElementPrecondition(const ElementConditionProto& proto,
                                          const std::vector<Result>& results,
                                          size_t* results_iter,
                                          std::vector<std::string>* payloads,
                                          std::vector<std::string>* tags);
  void CheckElementConditions();
  void OnResultsUpdated(const ClientStatus& status,
                        const std::vector<SelectorObserver::Update>& updates,
                        SelectorObserver* selector_observer);

  void OnGetElementsDone(const ClientStatus& status,
                         const base::flat_map<SelectorObserver::SelectorId,
                                              DomObjectFrameStack>& elements);

  // A way to find unique selectors and what |Result|'s are affected by them.
  //
  // Must not be modified after |Run()| is called because it's referenced by
  // index.
  base::flat_map<std::pair<Selector, /* strict */ bool>,
                 std::vector<std::pair</* element_condition_index */ size_t,
                                       /* result_index */ size_t>>>
      unique_selectors_;

  // Must not be modified after |Run()| is called because it's referenced by
  // index.
  std::vector<ElementConditionCheck> element_condition_checks_;

  // A map of GetFieldValue arguments (selector) to callbacks that take the
  // field value.
  base::flat_map<Selector, std::vector<GetFieldValueCallback>>
      get_field_value_callbacks_;
  int pending_checks_count_ = 0;

  // Run() was called. Checking elements might or might not have finished yet.
  bool started_ = false;

  // Whether to wait until one of the conditions becomes true. If it's a nullopt
  // it will check only once, if it has a value it will observe the DOM until
  // one of the conditions becomes true or the observation times out.
  absl::optional<const SelectorObserver::Settings> observer_settings_;

  std::vector<base::OnceCallback<void()>> all_done_;

  base::WeakPtrFactory<BatchElementChecker> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_BATCH_ELEMENT_CHECKER_H_
