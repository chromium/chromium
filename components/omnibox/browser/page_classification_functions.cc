// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/omnibox/browser/page_classification_functions.h"

namespace omnibox {
bool IsNTPPage(
    ::metrics::OmniboxEventProto::PageClassification classification) {
  using OEP = ::metrics::OmniboxEventProto;
  return (classification == OEP::NTP) ||
         (classification == OEP::OBSOLETE_INSTANT_NTP) ||
         (classification == OEP::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS) ||
         (classification == OEP::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS) ||
         (classification == OEP::NTP_REALBOX) ||
         (classification == OEP::NTP_ZPS_PREFETCH);
}

bool IsSearchResultsPage(
    ::metrics::OmniboxEventProto::PageClassification classification) {
  using OEP = ::metrics::OmniboxEventProto;
  return (classification ==
          OEP::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT) ||
         (classification ==
          OEP::SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT) ||
         (classification == OEP::SRP_ZPS_PREFETCH);
}

bool IsOtherWebPage(
    ::metrics::OmniboxEventProto::PageClassification classification) {
  using OEP = ::metrics::OmniboxEventProto;
  return (classification == OEP::OTHER) ||
         (classification == OEP::ANDROID_SHORTCUTS_WIDGET) ||
         (classification == OEP::OTHER_ZPS_PREFETCH);
}
}  // namespace omnibox
