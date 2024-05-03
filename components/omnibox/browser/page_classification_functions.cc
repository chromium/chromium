// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/omnibox/browser/page_classification_functions.h"

namespace omnibox {

using OEP = ::metrics::OmniboxEventProto;

bool IsNTPPage(OEP::PageClassification classification) {
  return (classification == OEP::NTP) ||
         (classification == OEP::OBSOLETE_INSTANT_NTP) ||
         (classification == OEP::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS) ||
         (classification == OEP::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS) ||
         (classification == OEP::NTP_REALBOX) ||
         (classification == OEP::NTP_ZPS_PREFETCH);
}

bool IsSearchResultsPage(OEP::PageClassification classification) {
  return (classification ==
          OEP::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT) ||
         (classification ==
          OEP::SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT) ||
         (classification == OEP::SEARCH_RESULT_PAGE_ON_CCT) ||
         (classification == OEP::SRP_ZPS_PREFETCH);
}

bool IsOtherWebPage(OEP::PageClassification classification) {
  return (classification == OEP::OTHER) ||
         (classification == OEP::OTHER_ON_CCT) ||
         (classification == OEP::ANDROID_SHORTCUTS_WIDGET) ||
         (classification == OEP::OTHER_ZPS_PREFETCH);
}

bool IsLensSearchbox(OEP::PageClassification classification) {
  return (classification == OEP::CONTEXTUAL_SEARCHBOX) ||
         (classification == OEP::SEARCH_SIDE_PANEL_SEARCHBOX) ||
         (classification == OEP::LENS_SIDE_PANEL_SEARCHBOX);
}

bool IsCustomTab(OEP::PageClassification classification) {
  return classification == OEP::SEARCH_RESULT_PAGE_ON_CCT ||
         classification == OEP::OTHER_ON_CCT;
}

}  // namespace omnibox
