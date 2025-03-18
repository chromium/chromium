// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CONTEXTUAL_SEARCH_ACTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CONTEXTUAL_SEARCH_ACTION_H_

#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "url/gurl.h"

// An action that triggers a contextual search via Lens.
class ContextualSearchAction : public OmniboxAction {
 public:
  ContextualSearchAction(const GURL& url,
                         AutocompleteMatchType::Type match_type,
                         bool is_zero_prefix_suggestion);

  // OmniboxAction::
  void RecordActionShown(size_t position, bool executed) const override;
  void Execute(ExecutionContext& context) const override;
  OmniboxActionId ActionId() const override;

 private:
  ~ContextualSearchAction() override;

  AutocompleteMatchType::Type match_type_;
  bool is_zero_prefix_suggestion_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CONTEXTUAL_SEARCH_ACTION_H_
