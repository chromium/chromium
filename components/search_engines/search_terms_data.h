// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"

// All data needed by TemplateURLRef::ReplaceSearchTerms which typically may
// only be accessed on the UI thread.
class SearchTermsData {
 public:
  SearchTermsData();
  virtual ~SearchTermsData();

  // Returns the value to use for replacements of type GOOGLE_BASE_URL.  This
  // implementation simply returns the default value.
  virtual std::string GoogleBaseURLValue() const;

  // Returns the value for the GOOGLE_BASE_SUGGEST_URL term.  This
  // implementation simply returns the default value.
  std::string GoogleBaseSuggestURLValue() const;

  // Returns the locale used by the application.  This implementation returns
  // "en" and thus should be overridden where the result is actually meaningful.
  virtual std::string GetApplicationLocale() const;

  // Returns the value for the Chrome Omnibox rlz.  This implementation returns
  // the empty string.
  virtual base::string16 GetRlzParameterValue(bool from_app_list) const;

  // The optional client parameter passed with Google search requests.  This
  // implementation returns the empty string.
  virtual std::string GetSearchClient() const;

  // The suggest client parameter ("client") passed with Google suggest
  // requests.  See GetSuggestRequestIdentifier() for more details.
  // |from_ntp| is true if the search is made from a non-searchbox NTP surface.
  // This implementation returns the empty string.
  virtual std::string GetSuggestClient(bool from_ntp) const;

  // The suggest request identifier parameter ("gs_ri") passed with Google
  // suggest requests.   Along with suggestclient (See GetSuggestClient()),
  // this parameter controls what suggestion results are returned.
  // This implementation returns the empty string.
  virtual std::string GetSuggestRequestIdentifier() const;

  // Returns the value to use for replacements of type
  // GOOGLE_IMAGE_SEARCH_SOURCE.
  virtual std::string GoogleImageSearchSource() const;

  // Returns the optional referral ID to be passed to Yandex when searching from
  // the omnibox (returns the empty string if not supported/applicable).
  virtual std::string GetYandexReferralID() const;

  // Returns the optional referral ID to be passed to @MAIL.RU when searching
  // from the omnibox (returns the empty string if not supported/applicable).
  virtual std::string GetMailRUReferralID() const;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  virtual size_t EstimateMemoryUsage() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchTermsData);
};

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_
