// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_DYNAMIC_TRIGGER_CONDITIONS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_DYNAMIC_TRIGGER_CONDITIONS_H_

#include <map>
#include <set>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/web/web_controller.h"

namespace autofill_assistant {

// Provides easy access to the values of dynamic trigger conditions. Dynamic
// trigger conditions depend on the current state of the DOM tree and need to be
// repeatedly re-evaluated.
class DynamicTriggerConditions {
 public:
  DynamicTriggerConditions();
  virtual ~DynamicTriggerConditions();

  // Adds the selector trigger conditions specified in |proto| to the list of
  // selectors to be queried in |Update|.
  virtual void AddSelectorsFromTriggerScript(const TriggerScriptProto& proto);

  // Clears all selectors from the list of selectors to be queried in |Update|.
  virtual void ClearSelectors();

  // Returns whether |selector| currently matches the DOM tree. |Update| must
  // be called prior to this method. Only selectors that have previously been
  // added via |AddSelectorsFromTriggerScript| can be queried.
  virtual base::Optional<bool> GetSelectorMatches(
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

  // Writes the result of the element lookup to |selector_matches_|. When all
  // |selectors_| have been evaluated, i.e., when the size of
  // |selector_matches_| is equal to the size of |selectors_|, invokes
  // |callback_|.
  void OnFindElement(const Selector& selector,
                     const ClientStatus& client_status,
                     std::unique_ptr<ElementFinder::Result> element);

  // Whether the keyboard is currently visible.
  bool keyboard_visible_ = false;
  // The current URL of the page.
  GURL url_;
  // Lookup cache for selector matches. Must be updated by invoking |Update|.
  std::map<Selector, bool> selector_matches_;
  // Temporary store for selector matches, used during |Update| as results
  // trickle in. Once all results have been gathered, this becomes the new
  // |selector_matches_|.
  std::map<Selector, bool> temporary_selector_matches_;
  // The list of selectors to look up on |Update|.
  std::set<Selector> selectors_;
  // The callback to invoke after |Update| is finished. Only set during
  // |Update|.
  base::OnceCallback<void(void)> callback_;
  base::WeakPtrFactory<DynamicTriggerConditions> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_DYNAMIC_TRIGGER_CONDITIONS_H_
