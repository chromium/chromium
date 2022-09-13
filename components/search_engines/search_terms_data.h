// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"

// All data needed by TemplateURLRef::ReplaceSearchTerms which typically may
// only be accessed on the UI thread.
class SearchTermsData {
 public:
  // Utility function that takes a snapshot of a different SearchTermsData
  // instance. This is used to access SearchTermsData off the UI thread, or to
  // copy the SearchTermsData for lifetime reasons.
  static std::unique_ptr<SearchTermsData> MakeSnapshot(
      const SearchTermsData* original_data);

  SearchTermsData();

  SearchTermsData(const SearchTermsData&) = delete;
  SearchTermsData& operator=(const SearchTermsData&) = delete;

  virtual ~SearchTermsData();

  // Returns the value to use for replacements of type GOOGLE_BASE_URL.  This
  // implementation simply returns the default value.
  virtual std::string GoogleBaseURLValue() const;

  // Returns the value to use for the GOOGLE_BASE_SEARCH_BY_IMAGE_URL. Points
  // at Lens if the user is enrolled in the Lens experiment, and defaults to
  // Image Search otherwise.
  virtual std::string GoogleBaseSearchByImageURLValue() const;

  // Returns the value for the GOOGLE_BASE_SUGGEST_URL term.  This
  // implementation simply returns the default value.
  std::string GoogleBaseSuggestURLValue() const;

  // Returns the locale used by the application.  This implementation returns
  // "en" and thus should be overridden where the result is actually meaningful.
  virtual std::string GetApplicationLocale() const;

  // Returns the value for the Chrome Omnibox rlz.  This implementation returns
  // the empty string.
  virtual std::u16string GetRlzParameterValue(bool from_app_list) const;

  // The optional client parameter passed with Google search requests.  This
  // implementation returns the empty string.
  virtual std::string GetSearchClient() const;

  // The suggest client parameter ("client") passed with Google suggest
  // requests.  See GetSuggestRequestIdentifier() for more details.
  // This implementation returns the empty string.
  virtual std::string GetSuggestClient(bool non_searchbox_ntp) const;

  // The suggest request identifier parameter ("gs_ri") passed with Google
  // suggest requests.   Along with suggestclient (See GetSuggestClient()),
  // this parameter controls what suggestion results are returned.
  // This implementation returns the empty string.
  virtual std::string GetSuggestRequestIdentifier(bool non_searchbox_ntp) const;

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
};

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_TERMS_DATA_H_
