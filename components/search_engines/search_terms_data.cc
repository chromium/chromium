// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_terms_data.h"

#include "base/logging.h"
#include "components/google/core/common/google_util.h"
#include "url/gurl.h"

SearchTermsData::SearchTermsData() {
}

SearchTermsData::~SearchTermsData() {
}

std::string SearchTermsData::GoogleBaseURLValue() const {
  return google_util::kGoogleHomepageURL;
}

std::string SearchTermsData::GoogleBaseSuggestURLValue() const {
  // Start with the Google base URL.
  const GURL base_url(GoogleBaseURLValue());
  DCHECK(base_url.is_valid());

  GURL::Replacements repl;

  // Replace any existing path with "/complete/".
  repl.SetPathStr("/complete/");

  // Clear the query and ref.
  repl.ClearQuery();
  repl.ClearRef();
  return base_url.ReplaceComponents(repl).spec();
}

std::string SearchTermsData::GetApplicationLocale() const {
  return "en";
}

base::string16 SearchTermsData::GetRlzParameterValue(bool from_app_list) const {
  return base::string16();
}

std::string SearchTermsData::GetSearchClient() const {
  return std::string();
}

std::string SearchTermsData::GetSuggestClient() const {
  return std::string();
}

std::string SearchTermsData::GetSuggestRequestIdentifier() const {
  return std::string();
}

std::string SearchTermsData::GoogleImageSearchSource() const {
  return std::string();
}

std::string SearchTermsData::GetYandexReferralID() const {
  return std::string();
}

std::string SearchTermsData::GetMailRUReferralID() const {
  return std::string();
}

size_t SearchTermsData::EstimateMemoryUsage() const {
  return 0;
}
