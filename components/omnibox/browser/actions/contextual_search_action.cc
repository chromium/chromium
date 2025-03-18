// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/contextual_search_action.h"

ContextualSearchAction::ContextualSearchAction(
    const GURL& url,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion)
    : OmniboxAction(LabelStrings(), url),
      match_type_(match_type),
      is_zero_prefix_suggestion_(is_zero_prefix_suggestion) {}

void ContextualSearchAction::RecordActionShown(size_t position,
                                               bool executed) const {
  // TODO(crbug.com/403644258): Add UMA logging.
}

void ContextualSearchAction::Execute(ExecutionContext& context) const {
  // Delegate fulfillment to Lens.
  context.client_->IssueContextualSearchRequest(url_, match_type_,
                                                is_zero_prefix_suggestion_);
}

OmniboxActionId ContextualSearchAction::ActionId() const {
  return OmniboxActionId::CONTEXTUAL_SEARCH;
}

ContextualSearchAction::~ContextualSearchAction() = default;
