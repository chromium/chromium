// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/contextual_search_action.h"

#include "components/strings/grit/components_strings.h"

ContextualSearchFulfillmentAction::ContextualSearchFulfillmentAction(
    const GURL& url,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion)
    : OmniboxAction(LabelStrings(), url),
      match_type_(match_type),
      is_zero_prefix_suggestion_(is_zero_prefix_suggestion) {}

void ContextualSearchFulfillmentAction::RecordActionShown(size_t position,
                                                          bool executed) const {
  // TODO(crbug.com/403644258): Add UMA logging.
}

void ContextualSearchFulfillmentAction::Execute(
    ExecutionContext& context) const {
  // Delegate fulfillment to Lens.
  context.client_->IssueContextualSearchRequest(url_, match_type_,
                                                is_zero_prefix_suggestion_);
}

OmniboxActionId ContextualSearchFulfillmentAction::ActionId() const {
  return OmniboxActionId::CONTEXTUAL_SEARCH_FULFILLMENT;
}

ContextualSearchFulfillmentAction::~ContextualSearchFulfillmentAction() =
    default;

////////////////////////////////////////////////////////////////////////////////

ContextualSearchAskAboutPageAction::ContextualSearchAskAboutPageAction()
    : OmniboxAction(OmniboxAction::LabelStrings(), GURL()) {}

OmniboxActionId ContextualSearchAskAboutPageAction::ActionId() const {
  return OmniboxActionId::CONTEXTUAL_SEARCH_ASK_ABOUT_PAGE;
}

void ContextualSearchAskAboutPageAction::Execute(
    ExecutionContext& context) const {
  context.client_->OpenLensOverlay(/*show=*/false);
}

ContextualSearchAskAboutPageAction::~ContextualSearchAskAboutPageAction() =
    default;

////////////////////////////////////////////////////////////////////////////////

ContextualSearchSelectRegionAction::ContextualSearchSelectRegionAction()
    : OmniboxAction(OmniboxAction::LabelStrings(), GURL()) {}

OmniboxActionId ContextualSearchSelectRegionAction::ActionId() const {
  return OmniboxActionId::CONTEXTUAL_SEARCH_SELECT_REGION;
}

void ContextualSearchSelectRegionAction::Execute(
    ExecutionContext& context) const {
  context.client_->OpenLensOverlay(/*show=*/true);
}

ContextualSearchSelectRegionAction::~ContextualSearchSelectRegionAction() =
    default;
