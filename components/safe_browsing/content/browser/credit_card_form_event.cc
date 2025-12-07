// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/credit_card_form_event.h"

#include <optional>

#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace safe_browsing::credit_card_form {

namespace {

void LogCreditCardFormEvent(std::string_view event_name,
                            CreditCardFormEvent event) {
  base::UmaHistogramSparse(
      base::StrCat({"SBClientPhishing.CreditCardFormEvent2.", event_name}),
      static_cast<int>(event));
}

}  // namespace

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

#if BUILDFLAG(IS_ANDROID)
ReferringApp FromReferringAppInfo(internal::ReferringAppInfo info) {
  static constexpr char kAndroidAppPrefix[] = "android-app://";
  static constexpr auto kSmsApps = base::MakeFixedFlatSet<std::string_view>({
      "android.messages",
      "com.samsung.android.messaging",
  });
  if (!info.has_referring_app()) {
    return kNoReferringApp;
  }
  std::string_view app_name = info.referring_app_name;
  if (app_name.starts_with(kAndroidAppPrefix)) {
    app_name.remove_prefix(strlen(kAndroidAppPrefix));
  }
  if (app_name == "chrome") {
    return kChrome;
  }
  if (kSmsApps.contains(app_name)) {
    return kSmsApp;
  }
  return kOtherApp;
}

void LogEvent(std::string_view event_name,
              SiteVisit site_visit,
              ReferringApp referring_app,
              FieldDetectionHeuristic field_heuristic) {
  CreditCardFormEvent event =
      GetCreditCardFormEvent(site_visit, referring_app, field_heuristic);
  LogCreditCardFormEvent(event_name, event);
}

#endif

void LogEvent(std::string_view event_name,
              SiteVisit site_visit,
              FieldDetectionHeuristic field_heuristic) {
  CreditCardFormEvent event =
      GetCreditCardFormEvent(site_visit, kNoReferringApp, field_heuristic);
  LogCreditCardFormEvent(event_name, event);
}

}  // namespace safe_browsing::credit_card_form
