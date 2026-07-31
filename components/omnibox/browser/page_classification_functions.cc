// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/omnibox/browser/page_classification_functions.h"

#include "base/check.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "page_classification_functions.h"

namespace omnibox {

using OEP = ::metrics::OmniboxEventProto;

bool IsNTPPage(OEP::PageClassification classification) {
  CheckObsoletePageClass(classification);

  return (classification == OEP::NTP) ||
         (classification == OEP::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS) ||
         (classification == OEP::NTP_REALBOX) ||
         (classification == OEP::NTP_ZPS_PREFETCH);
}

void CheckObsoletePageClass(OEP::PageClassification classification) {
  CHECK(classification != OEP::OBSOLETE_INSTANT_NTP &&
        classification !=
            OEP::OBSOLETE_INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS)
      << "crbug.com/357961079: invalid/unexpected page context. Please report.";
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

bool IsNtpOmnibox(OEP::PageClassification classification) {
  return (classification == OEP::NTP) ||
         (classification == OEP::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS);
}

bool IsWebUISearchbox(OEP::PageClassification classification) {
  return classification == OEP::NTP_REALBOX ||
         classification == OEP::OMNIBOX_EVERYWHERE ||
         IsLensSearchbox(classification) || IsComposebox(classification);
}

// COMPOSEBOX
bool IsComposebox(OEP::PageClassification classification) {
  return classification == OEP::NTP_COMPOSEBOX ||
         classification == OEP::LENS_SIDE_PANEL_COMPOSEBOX ||
         classification == OEP::CO_BROWSING_COMPOSEBOX ||
         classification == OEP::NTP_COMPOSEBOX_PREFETCH ||
         IsOmniboxComposebox(classification);
}

bool IsOmniboxComposebox(OEP::PageClassification classification) {
  return classification == OEP::NTP_OMNIBOX_COMPOSEBOX ||
         classification == OEP::SRP_OMNIBOX_COMPOSEBOX ||
         classification == OEP::OTHER_OMNIBOX_COMPOSEBOX ||
         classification == OEP::COMPOSEBOX_EVERYWHERE;
}

bool IsNTPComposebox(OEP::PageClassification classification) {
  return classification == OEP::NTP_COMPOSEBOX ||
         classification == OEP::NTP_OMNIBOX_COMPOSEBOX ||
         classification == OEP::NTP_COMPOSEBOX_PREFETCH;
}

// LENS
bool IsLensSearchbox(OEP::PageClassification classification) {
  return classification == OEP::CONTEXTUAL_SEARCHBOX ||
         classification == OEP::SEARCH_SIDE_PANEL_SEARCHBOX ||
         classification == OEP::LENS_SIDE_PANEL_SEARCHBOX ||
         classification == OEP::LENS_SIDE_PANEL_COMPOSEBOX;
}

// ANDROID ONLY
bool IsCustomTab(OEP::PageClassification classification) {
  return classification == OEP::SEARCH_RESULT_PAGE_ON_CCT ||
         classification == OEP::OTHER_ON_CCT;
}

bool IsAndroidHubOrTabSearch(OEP::PageClassification classification) {
  return classification == OEP::ANDROID_HUB ||
         classification == OEP::ANDROID_TAB_SEARCH_OVERLAY;
}

bool IsAndroidWidget(OEP::PageClassification classification) {
  return (classification == OEP::ANDROID_SHORTCUTS_WIDGET ||
          classification == OEP::ANDROID_SEARCH_WIDGET);
}

// FEATURE SUPPORT
bool SupportsMostVisitedSites(OEP::PageClassification classification) {
  if (omnibox_feature_configs::OmniboxUrlSuggestionsOnFocus::Get().enabled) {
    return classification == OEP::OTHER_ON_CCT ||
           classification == OEP::OTHER ||
           classification == OEP::OTHER_ZPS_PREFETCH ||
           IsSearchResultsPage(classification);
  }

  return classification == OEP::OTHER ||
         classification == OEP::ANDROID_SEARCH_WIDGET ||
         classification == OEP::ANDROID_SHORTCUTS_WIDGET;
}


}  // namespace omnibox
