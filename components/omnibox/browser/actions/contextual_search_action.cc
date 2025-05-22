// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/contextual_search_action.h"

#include "components/omnibox/common/omnibox_feature_configs.h"
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
    : OmniboxAction(OmniboxAction::LabelStrings(
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
