// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/credit_card_form_event.h"

#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace safe_browsing::credit_card_form {

CreditCardFormEvent GetCreditCardFormEvent(SiteVisit site_visit,
                                           ReferringApp referring_app,
                                           FieldDetectionHeuristic heuristic) {
  // CreditCardFormEvent is a sparse enum representing all permutations
  // of the input enums, where each input enum value contributes an order of
  // magnitude to the permutation value.
  // This math works, because each enum value is expected not to exceed having
  // ten values.
  int ordinal = 100 * site_visit + 10 * referring_app + heuristic;
  DCHECK(ordinal <= kCreditCardFormEventMaxValue);
  return static_cast<CreditCardFormEvent>(ordinal);
}

void LogEvent(std::string_view event_name, SiteVisit site_visit) {
  // Use these values until parameters are added to specify the correct values.
  ReferringApp referring_app = kNoReferringApp;
  FieldDetectionHeuristic heuristic = kNoDetectionHeuristic;

  CreditCardFormEvent event =
      GetCreditCardFormEvent(site_visit, referring_app, heuristic);

  base::UmaHistogramSparse(
      base::StrCat({"SBClientPhishing.CreditCardFormEvent.", event_name}),
      static_cast<int>(event));
}

}  // namespace safe_browsing::credit_card_form
