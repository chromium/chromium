// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace critical_actions {

std::string_view ActionSourceToHistogramSuffix(ActionSource source) {
  switch (source) {
    case ActionSource::kPasswordManager:
      return "PasswordManager";
    case ActionSource::kActor:
      return "Actor";
    case ActionSource::kAutofill:
      return "Autofill";
    case ActionSource::kUnknown:
      return "Unknown";
  }
  NOTREACHED();
}

void RecordVisitIdResolutionOutcome(ActionSource source,
                                    VisitIdResolutionOutcome outcome) {
  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalActions.VisitIdResolutionOutcome.",
                    ActionSourceToHistogramSuffix(source)}),
      outcome);
}

void RecordConversationAnchorResolutionOutcome(
    ActionSource source,
    ConversationAnchorResolutionOutcome outcome) {
  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalActions.ConversationAnchorResolutionOutcome.",
                    ActionSourceToHistogramSuffix(source)}),
      outcome);
}

void RecordConversationIdResolutionOutcome(
    ActionSource source,
    ConversationIdResolutionOutcome outcome) {
  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalActions.ConversationIdResolutionOutcome.",
                    ActionSourceToHistogramSuffix(source)}),
      outcome);
}

void RecordConversationIdDeferredResolutionOutcome(
    ActionSource source,
    ConversationIdDeferredResolutionOutcome outcome) {
  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalActions.ConversationIdDeferredResolutionOutcome.",
                    ActionSourceToHistogramSuffix(source)}),
      outcome);
}

void RecordEventLogged(ActionSource source, ActionType action_type) {
  base::UmaHistogramEnumeration(
      base::StrCat({"CriticalActions.EventLogged.",
                    ActionSourceToHistogramSuffix(source)}),
      action_type);
}

}  // namespace critical_actions
