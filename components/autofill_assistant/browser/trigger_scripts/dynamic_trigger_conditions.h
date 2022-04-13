// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_DYNAMIC_TRIGGER_CONDITIONS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_DYNAMIC_TRIGGER_CONDITIONS_H_

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// Provides easy access to the values of dynamic trigger conditions. Dynamic
// trigger conditions depend on the current state of the DOM tree and need to be
// repeatedly re-evaluated.
class DynamicTriggerConditions {
 public:
  DynamicTriggerConditions();
  virtual ~DynamicTriggerConditions();

  // Adds the trigger conditions specified in |proto| to the list of conditions
  // to check for in |Update|.
  virtual void AddConditionsFromTriggerScript(const TriggerScriptProto& proto);

  // Clears all conditions previously added by |AddConditionsFromTriggerScript|.
  virtual void ClearConditions();

  // Returns whether |selector| currently matches the DOM tree. |Update| must
  // be called prior to this method. Only selectors that have previously been
  // added via |AddConditionsFromTriggerScript| can be queried.
  virtual absl::optional<bool> GetSelectorMatches(
      const Selector& selector) const;

  // Sets whether the keyboard is currently visible.
  virtual void SetKeyboardVisible(bool visible);

  // Returns whether the keyboard is currently visible.
  virtual bool GetKeyboardVisible() const;

  // Updates the current URL of the page.
  virtual void SetURL(const GURL& url);

  // Returns whether the current URL matches the given path pattern.
  virtual bool GetPathPatternMatches(const std::string& path_pattern) const;

  // Returns whether the current URL belongs to the given domain.
  virtual bool GetDomainAndSchemeMatches(const GURL& domain_with_scheme) const;

  // Returns the current document ready state for |frame|. Returns the ready
  // state for the main frame if an empty selector is provided. Returns
  // absl::nullopt if this is called for a |frame| that was not requested in any
  // of the trigger conditions.
  virtual absl::optional<DocumentReadyState> GetDocumentReadyState(
      const Selector& frame) const;

  // Matches all previously added selectors with the current DOM tree and caches
  // the results to be available via |GetSelectorMatches|. Invokes |callback|
  // when done.
  //
  // NOTE: this class is not thread-safe. Don't invoke any of its methods while
  // |Update| is running.
  virtual void Update(WebController* web_controller,
                      base::OnceCallback<void(void)> callback);

  // If true, all values have been evaluated. They may be out-of-date by one
  // cycle in case an update is currently scheduled.
  virtual bool HasResults() const;

 private:
  friend class DynamicTriggerConditionsTest;

  // Writes the result of the element lookup to |temporary_selector_matches_|.
  // When all selectors have been evaluated invokes |MaybeRunCallback|.
  void OnFindElement(const Selector& selector,
                     const ClientStatus& client_status,
                     std::unique_ptr<ElementFinderResult> element);

  // Writes the result of the operation to |temporary_dom_ready_states_|. When
  // all dom ready states are evaluated, invokes |MaybeRunCallback|.
  void OnGetDocumentReadyState(const Selector& selector,
                               const ClientStatus& client_status,
                               DocumentReadyState document_ready_state);

  // Determines whether the current call to Update is complete. If yes, invokes
  // the callback and finishes the update.
  void MaybeRunCallback();

  // Whether the keyboard is currently visible.
  bool keyboard_visible_ = false;
  // The current URL of the page.
  GURL url_;
  // Lookup cache for selector matches. Must be updated by invoking |Update|.
  base::flat_map<Selector, bool> selector_matches_;
  // Temporary store for selector matches, used during |Update| as results
  // trickle in. Once all results have been gathered, this becomes the new
  // |selector_matches_|.
  base::flat_map<Selector, bool> temporary_selector_matches_;
  // Lookup cache for document ready states for specific frames.
  base::flat_map<Selector, DocumentReadyState> dom_ready_states_;
  // Temporary store for document ready states for specific frames, used during
  // |Update| as results trickle in. Once all results have been gathered, this
  // becomes the new |dom_ready_states_|.
  base::flat_map<Selector, DocumentReadyState> temporary_dom_ready_states_;
  // The list of selectors to look up on |Update|.
  base::flat_set<Selector> selectors_;
  // The list of frames for which to evaluate the dom ready state.
  base::flat_set<Selector> dom_ready_state_selectors_;
  // The callback to invoke after |Update| is finished. Only set during
  // |Update|.
  base::OnceCallback<void(void)> callback_;
  base::WeakPtrFactory<DynamicTriggerConditions> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_DYNAMIC_TRIGGER_CONDITIONS_H_
