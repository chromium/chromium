// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CONTEXTUAL_SEARCH_ACTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CONTEXTUAL_SEARCH_ACTION_H_

#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "url/gurl.h"

// An action that fulfills a contextual search via Lens.
class ContextualSearchFulfillmentAction : public OmniboxAction {
 public:
  ContextualSearchFulfillmentAction(const GURL& url,
                                    AutocompleteMatchType::Type match_type,
                                    bool is_zero_prefix_suggestion);

  // OmniboxAction:
  void RecordActionShown(size_t position, bool executed) const override;
  void Execute(ExecutionContext& context) const override;
  OmniboxActionId ActionId() const override;

 private:
  ~ContextualSearchFulfillmentAction() override;

  AutocompleteMatchType::Type match_type_;
  bool is_zero_prefix_suggestion_;
};

class ContextualSearchOpenLensAction : public OmniboxAction {
 public:
  ContextualSearchOpenLensAction();

  // OmniboxAction:
  OmniboxActionId ActionId() const override;
  void Execute(ExecutionContext& context) const override;
#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif

 protected:
  ~ContextualSearchOpenLensAction() override;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CONTEXTUAL_SEARCH_ACTION_H_
