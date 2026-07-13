// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/contextual_search_action.h"

#include "base/metrics/histogram_functions.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

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
    : OmniboxAction(
          LabelStrings(
              IDS_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION_HINT,
              IDS_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION_SUGGESTION_CONTENTS,
              IDS_ACC_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION_SUFFIX,
              IDS_ACC_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION),
          url),
      match_type_(match_type),
      is_zero_prefix_suggestion_(is_zero_prefix_suggestion) {}

OmniboxActionId ContextualSearchFulfillmentAction::ActionId() const {
  return OmniboxActionId::CONTEXTUAL_SEARCH_FULFILLMENT;
}

void ContextualSearchFulfillmentAction::RecordActionShown(size_t position,
                                                          bool executed) const {
  base::UmaHistogramBoolean("Omnibox.ContextualSearchFulfillmentAction.Ctr",
                            executed);
}

void ContextualSearchFulfillmentAction::Execute(
    ExecutionContext& context) const {
  // Delegate fulfillment to Lens. Navigate to the action's `fulfillment_url_`
  // if valid, since this url includes the associated match's updated
  // `search_terms_args` which are attached to the match after this action is
  // added to it.
  // NOTE: `url_` should only be navigated to for the pedal fulfillment since
  // there are not any searchbox stats associated with the pedal match.
  context.client_->IssueContextualSearchRequest(
      fulfillment_url_.is_valid() ? fulfillment_url_ : url_, match_type_,
      is_zero_prefix_suggestion_);
}

ContextualSearchFulfillmentAction*
ContextualSearchFulfillmentAction::FromAction(OmniboxAction* action) {
  if (action &&
      action->ActionId() == OmniboxActionId::CONTEXTUAL_SEARCH_FULFILLMENT) {
    return static_cast<ContextualSearchFulfillmentAction*>(action);
  }
  return nullptr;
}
#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
const gfx::VectorIcon& ContextualSearchFulfillmentAction::GetVectorIcon()
    const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kGoogleLensMonochromeLogoIcon;
#else
  return features::IsRoundedIconsEnabled()
             ? vector_icons::kSearchIcon
             : vector_icons::kSearchChromeRefreshOldIcon;
#endif
}
#endif  // defined(SUPPORT_PEDALS_VECTOR_ICONS)

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
                    GetOpenLensActionLabelId(),
                    GetOpenLensActionLabelId(),
                    IDS_ACC_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION_SUFFIX,
                    IDS_ACC_CONTEXTUAL_SEARCH_OPEN_LENS_ACTION),
          GURL()) {}

OmniboxActionId ContextualSearchOpenLensAction::ActionId() const {
  return OmniboxActionId::CONTEXTUAL_SEARCH_OPEN_LENS;
}

void ContextualSearchOpenLensAction::RecordActionShown(size_t position,
                                                       bool executed) const {
  base::UmaHistogramBoolean("Omnibox.ContextualSearchOpenLensAction.Ctr",
                            executed);
}

void ContextualSearchOpenLensAction::Execute(ExecutionContext& context) const {
  if (context.client_->ShouldOpenCoBrowsePanel()) {
    context.client_->OpenCoBrowsePanel();
  } else {
    context.client_->OpenLensOverlay(/*show=*/true);
  }
}

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
const gfx::VectorIcon& ContextualSearchOpenLensAction::GetVectorIcon() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return omnibox_feature_configs::ContextualSearch::Get()
                 .open_lens_action_ui_tweaks
             ? vector_icons::kGoogleLensLogoIcon
             : vector_icons::kGoogleLensMonochromeLogoIcon;
#else
  return features::IsRoundedIconsEnabled()
             ? vector_icons::kSearchIcon
             : vector_icons::kSearchChromeRefreshOldIcon;
#endif
}
#endif  // defined(SUPPORT_PEDALS_VECTOR_ICONS)

ContextualSearchOpenLensAction::~ContextualSearchOpenLensAction() = default;
