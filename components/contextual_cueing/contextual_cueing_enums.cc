// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_cueing/contextual_cueing_enums.h"

namespace contextual_cueing {

const char* GetName(ContextualCueingDecision decision) {
  switch (decision) {
    case ContextualCueingDecision::kUnspecified:
      return "Unspecified";
    case ContextualCueingDecision::
        kNoLongerActiveTabAfterCategoryClassification:
      return "NoLongerActiveTabAfterCategoryClassification";
    case ContextualCueingDecision::kFailedCategoryClassification:
      return "FailedCategoryClassification";
    case ContextualCueingDecision::kModelExecutionUnavailable:
      return "ModelExecutionUnavailable";
    case ContextualCueingDecision::kModelExecutionFailed:
      return "ModelExecutionFailed";
    case ContextualCueingDecision::kModelExecutionResponseFailedToParse:
      return "ModelExecutionResponseFailedToParse";
    case ContextualCueingDecision::kSuccess:
      return "Success";
    case ContextualCueingDecision::kMissingAnchoredMessageText:
      return "MissingAnchoredMessageText";
    case ContextualCueingDecision::kUnknownFulfillmentSurface:
      return "UnknownFulfillmentSurface";
    case ContextualCueingDecision::kTargetFeatureNotRegistered:
      return "TargetFeatureNotRegistered";
    case ContextualCueingDecision::kTargetFeatureNotEligible:
      return "TargetFeatureNotEligible";
    case ContextualCueingDecision::kNoActiveTab:
      return "NoActiveTab";
    case ContextualCueingDecision::kNoPageActions:
      return "NoPageActions";
    case ContextualCueingDecision::kNoLongerActiveTabAfterModelExecution:
      return "NoLongerActiveTabAfterModelExecution";
    case ContextualCueingDecision::kFeaturePromoActive:
      return "FeaturePromoActive";
    case ContextualCueingDecision::kHistorySyncOff:
      return "HistorySyncOff";
    case ContextualCueingDecision::kNotEnoughPageLoadsSinceLastCue:
      return "NotEnoughPageLoadsSinceLastCue";
    case ContextualCueingDecision::kNotEnoughTimeSinceLastCue:
      return "NotEnoughTimeSinceLastCue";
    case ContextualCueingDecision::kTooManyCuesShownToTheUser:
      return "TooManyCuesShownToTheUser";
    case ContextualCueingDecision::kTooManyCuesShownToTheUserForOrigin:
      return "TooManyCuesShownToTheUserForOrigin";
    case ContextualCueingDecision::kUrlNotEligible:
      return "UrlNotEligible";
    case ContextualCueingDecision::kNotEnoughTimeSinceLastDismissal:
      return "NotEnoughTimeSinceLastDismissal";
    case ContextualCueingDecision::kSidePanelShowing:
      return "SidePanelShowing";
    case ContextualCueingDecision::kNoEligibleCueSurfaces:
      return "NoEligibleCueSurfaces";
    case ContextualCueingDecision::kInfobarVisible:
      return "InfobarVisible";
    case ContextualCueingDecision::kUserOptedOut:
      return "UserOptedOut";
    case ContextualCueingDecision::kDisabledByEnterprisePolicy:
      return "DisabledByEnterprisePolicy";
    case ContextualCueingDecision::kAgeRestrictionEnforced:
      return "AgeRestrictionEnforced";
    case ContextualCueingDecision::kNoCues:
      return "NoCues";
    case ContextualCueingDecision::kNotEnoughTimeSinceLastClick:
      return "NotEnoughTimeSinceLastClick";
    case ContextualCueingDecision::kAnchoredMessageAlreadyShowing:
      return "AnchoredMessageAlreadyShowing";
    case ContextualCueingDecision::kTabInSplitView:
      return "TabInSplitView";
    case ContextualCueingDecision::kWebContentsDestroyed:
      return "WebContentsDestroyed";
    case ContextualCueingDecision::kNoLongerActiveTabAfterEligibilityCheck:
      return "NoLongerActiveTabAfterEligibilityCheck";
  }
}

const char* GetName(ContextualCueingInteraction interaction) {
  switch (interaction) {
    case ContextualCueingInteraction::kCueClicked:
      return "CueClicked";
    case ContextualCueingInteraction::kCueDismissed:
      return "CueDismissed";
    case ContextualCueingInteraction::kCueEditPrompt:
      return "CueEditPrompt";
    case ContextualCueingInteraction::kCueSuggestionsSettings:
      return "CueSuggestionsSettings";
  }
}

const char* GetName(CueFormFactor form_factor) {
  switch (form_factor) {
    case CueFormFactor::kIcon:
      return "Icon";
    case CueFormFactor::kChip:
      return "Chip";
    case CueFormFactor::kAnchoredMessage:
      return "AnchoredMessage";
  }
}

}  // namespace contextual_cueing
