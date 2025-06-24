// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/contextual_search_action.h"

#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
#include "build/branding_buildflags.h"                // nogncheck
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif

namespace {

int GetOpenLensActionLabelId() {
  switch (omnibox_feature_configs::ContextualSearch::Get()
              .alternative_action_label) {
    case 1:
      return IDS_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION_LABEL_ALT;
    case 2:
      return IDS_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION_LABEL_ALT2;
    case 0:
    default:
      return IDS_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION_LABEL;
  }
}

}  // namespace

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

ContextualSearchOpenLensAction::ContextualSearchOpenLensAction()
    : OmniboxAction(
          omnibox_feature_configs::Toolbelt::Get().enabled
              ? OmniboxAction::LabelStrings(
                    IDS_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION_HINT,
                    IDS_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION_SUGGESTION_CONTENTS,
                    IDS_ACC_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION_SUFFIX,
                    IDS_ACC_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION)
              : OmniboxAction::LabelStrings(
                    l10n_util::GetStringUTF16(GetOpenLensActionLabelId()),
                    u"",
                    u"",
                    u""),
          GURL()) {}

OmniboxActionId ContextualSearchOpenLensAction::ActionId() const {
  return OmniboxActionId::CONTEXTUAL_SEARCH_OPEN_LENS;
}

void ContextualSearchOpenLensAction::Execute(ExecutionContext& context) const {
  context.client_->OpenLensOverlay(/*show=*/true);
}

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
const gfx::VectorIcon& ContextualSearchOpenLensAction::GetVectorIcon() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kGoogleLensMonochromeLogoIcon;
#else
  return vector_icons::kSearchChromeRefreshIcon;
#endif
}
#endif  // defined(SUPPORT_PEDALS_VECTOR_ICONS)

ContextualSearchOpenLensAction::~ContextualSearchOpenLensAction() = default;

////////////////////////////////////////////////////////////////////////////////

StarterPackBookmarksAction::StarterPackBookmarksAction()
    : OmniboxAction(OmniboxAction::LabelStrings(
                        IDS_STARTER_PACK_BOOKMARKS_ACTION_HINT,
                        IDS_STARTER_PACK_BOOKMARKS_ACTION_SUGGESTION_CONTENTS,
                        IDS_ACC_STARTER_PACK_BOOKMARKS_ACTION_SUFFIX,
                        IDS_ACC_STARTER_PACK_BOOKMARKS_ACTION),
                    GURL()) {}

OmniboxActionId StarterPackBookmarksAction::ActionId() const {
  return OmniboxActionId::STARTER_PACK_BOOKMARKS;
}

void StarterPackBookmarksAction::Execute(ExecutionContext& context) const {
  context.enter_starter_pack_id_ =
      template_url_starter_pack_data::StarterPackId::kBookmarks;
}

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
const gfx::VectorIcon& StarterPackBookmarksAction::GetVectorIcon() const {
  return omnibox::kStarActiveChromeRefreshIcon;
}
#endif  // defined(SUPPORT_PEDALS_VECTOR_ICONS)

StarterPackBookmarksAction::~StarterPackBookmarksAction() = default;

////////////////////////////////////////////////////////////////////////////////

StarterPackHistoryAction::StarterPackHistoryAction()
    : OmniboxAction(OmniboxAction::LabelStrings(
                        IDS_STARTER_PACK_HISTORY_ACTION_HINT,
                        IDS_STARTER_PACK_HISTORY_ACTION_SUGGESTION_CONTENTS,
                        IDS_ACC_STARTER_PACK_HISTORY_ACTION_SUFFIX,
                        IDS_ACC_STARTER_PACK_HISTORY_ACTION),
                    GURL()) {}

OmniboxActionId StarterPackHistoryAction::ActionId() const {
  return OmniboxActionId::STARTER_PACK_HISTORY;
}

void StarterPackHistoryAction::Execute(ExecutionContext& context) const {
  context.enter_starter_pack_id_ =
      template_url_starter_pack_data::StarterPackId::kHistory;
}

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
const gfx::VectorIcon& StarterPackHistoryAction::GetVectorIcon() const {
  return vector_icons::kHistoryChromeRefreshIcon;
}
#endif  // defined(SUPPORT_PEDALS_VECTOR_ICONS)

StarterPackHistoryAction::~StarterPackHistoryAction() = default;

////////////////////////////////////////////////////////////////////////////////

StarterPackTabsAction::StarterPackTabsAction()
    : OmniboxAction(OmniboxAction::LabelStrings(
                        IDS_STARTER_PACK_TABS_ACTION_HINT,
                        IDS_STARTER_PACK_TABS_ACTION_SUGGESTION_CONTENTS,
                        IDS_ACC_STARTER_PACK_TABS_ACTION_SUFFIX,
                        IDS_ACC_STARTER_PACK_TABS_ACTION),
                    GURL()) {}

OmniboxActionId StarterPackTabsAction::ActionId() const {
  return OmniboxActionId::STARTER_PACK_TABS;
}

void StarterPackTabsAction::Execute(ExecutionContext& context) const {
  context.enter_starter_pack_id_ =
      template_url_starter_pack_data::StarterPackId::kTabs;
}

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
const gfx::VectorIcon& StarterPackTabsAction::GetVectorIcon() const {
  return omnibox::kProductChromeRefreshIcon;
}
#endif  // defined(SUPPORT_PEDALS_VECTOR_ICONS)

StarterPackTabsAction::~StarterPackTabsAction() = default;

////////////////////////////////////////////////////////////////////////////////

StarterPackAiModeAction::StarterPackAiModeAction()
    : OmniboxAction(OmniboxAction::LabelStrings(
                        IDS_STARTER_PACK_AI_MODE_ACTION_HINT,
                        IDS_STARTER_PACK_AI_MODE_ACTION_SUGGESTION_CONTENTS,
                        IDS_ACC_STARTER_PACK_AI_MODE_ACTION_SUFFIX,
                        IDS_ACC_STARTER_PACK_AI_MODE_ACTION),
                    GURL()) {}

OmniboxActionId StarterPackAiModeAction::ActionId() const {
  return OmniboxActionId::STARTER_PACK_AI_MODE;
}

void StarterPackAiModeAction::Execute(ExecutionContext& context) const {
  context.enter_starter_pack_id_ =
      template_url_starter_pack_data::StarterPackId::kAiMode;
}

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
const gfx::VectorIcon& StarterPackAiModeAction::GetVectorIcon() const {
  return omnibox::kSearchSparkIcon;
}
#endif  // defined(SUPPORT_PEDALS_VECTOR_ICONS)

StarterPackAiModeAction::~StarterPackAiModeAction() = default;
