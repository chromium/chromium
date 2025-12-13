// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CONTEXTUAL_SEARCH_ACTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CONTEXTUAL_SEARCH_ACTION_H_

#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "url/gurl.h"

// An action that fulfills a contextual search via the Lens CSB flow.
// This action is specified as the `takeover_action` for contextual search
// matches in order to trigger fulfillment via the Lens CSB flow.
class ContextualSearchFulfillmentAction : public OmniboxAction {
 public:
  ContextualSearchFulfillmentAction(const GURL& url,
                                    AutocompleteMatchType::Type match_type,
                                    bool is_zero_prefix_suggestion);

  // OmniboxAction:
  OmniboxActionId ActionId() const override;
  void RecordActionShown(size_t position, bool executed) const override;
  void Execute(ExecutionContext& context) const override;
#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif

  static ContextualSearchFulfillmentAction* FromAction(OmniboxAction* action);

  void set_fulfillment_url(GURL url) { fulfillment_url_ = std::move(url); }

  const GURL get_fulfillment_url_for_testing() { return fulfillment_url_; }

 protected:
  ~ContextualSearchFulfillmentAction() override;

  AutocompleteMatchType::Type match_type_;
  bool is_zero_prefix_suggestion_;
  // This URL includes all of the search_terms_args that its match had
  // associated with it.
  GURL fulfillment_url_;
};

// An action that invokes the Lens overlay UI for the current page.
// This action will be shown as either a standalone suggestion in the Omnibox
// popup or a dedicated action in the Omnibox toolbelt.
class ContextualSearchOpenLensAction : public OmniboxAction {
 public:
  ContextualSearchOpenLensAction();

  // OmniboxAction:
  OmniboxActionId ActionId() const override;
  void RecordActionShown(size_t position, bool executed) const override;
  void Execute(ExecutionContext& context) const override;
#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif

 protected:
  ~ContextualSearchOpenLensAction() override;
};

class StarterPackBookmarksAction : public OmniboxAction {
 public:
  StarterPackBookmarksAction();

  // OmniboxAction:
  OmniboxActionId ActionId() const override;
  void RecordActionShown(size_t position, bool executed) const override;
  void Execute(ExecutionContext& context) const override;
#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif

 protected:
  ~StarterPackBookmarksAction() override;
};

class StarterPackHistoryAction : public OmniboxAction {
 public:
  StarterPackHistoryAction();

  // OmniboxAction:
  OmniboxActionId ActionId() const override;
  void RecordActionShown(size_t position, bool executed) const override;
  void Execute(ExecutionContext& context) const override;
#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif

 protected:
  ~StarterPackHistoryAction() override;
};

class StarterPackTabsAction : public OmniboxAction {
 public:
  StarterPackTabsAction();

  // OmniboxAction:
  OmniboxActionId ActionId() const override;
  void RecordActionShown(size_t position, bool executed) const override;
  void Execute(ExecutionContext& context) const override;
#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif

 protected:
  ~StarterPackTabsAction() override;
};

class StarterPackAiModeAction : public OmniboxAction {
 public:
  StarterPackAiModeAction();

  // OmniboxAction:
  OmniboxActionId ActionId() const override;
  void RecordActionShown(size_t position, bool executed) const override;
  void Execute(ExecutionContext& context) const override;
#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif

 protected:
  ~StarterPackAiModeAction() override;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_CONTEXTUAL_SEARCH_ACTION_H_
