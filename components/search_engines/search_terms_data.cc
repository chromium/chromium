// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_terms_data.h"
#include <memory>

#include "base/check.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/google/core/common/google_util.h"
#include "components/lens/lens_features.h"
#include "url/gurl.h"

namespace {

// -----------------------------------------------------------------
// SearchTermsDataSnapshot

// Implementation of SearchTermsData that takes a snapshot of another
// SearchTermsData by copying all the responses to the different getters into
// member strings, then returning those strings when its own getters are called.
// This will typically be constructed on the UI thread from
// UIThreadSearchTermsData but is subsequently safe to use on any thread.
class SearchTermsDataSnapshot : public SearchTermsData {
 public:
  explicit SearchTermsDataSnapshot(const SearchTermsData* search_terms_data);
  ~SearchTermsDataSnapshot() override;
  SearchTermsDataSnapshot(const SearchTermsDataSnapshot&) = delete;
  SearchTermsDataSnapshot& operator=(const SearchTermsDataSnapshot&) = delete;

  std::string GoogleBaseURLValue() const override;
  std::string GetApplicationLocale() const override;
  std::u16string GetRlzParameterValue(bool from_app_list) const override;
  std::string GetSearchClient() const override;
  std::string GoogleImageSearchSource() const override;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const override;

 private:
  std::string google_base_url_value_;
  std::string application_locale_;
  std::u16string rlz_parameter_value_;
  std::string search_client_;
  std::string google_image_search_source_;
};

SearchTermsDataSnapshot::SearchTermsDataSnapshot(
    const SearchTermsData* search_terms_data) {
  if (search_terms_data) {
    google_base_url_value_ = search_terms_data->GoogleBaseURLValue();
    application_locale_ = search_terms_data->GetApplicationLocale();
    rlz_parameter_value_ = search_terms_data->GetRlzParameterValue(false);
    search_client_ = search_terms_data->GetSearchClient();
    google_image_search_source_ = search_terms_data->GoogleImageSearchSource();
  }
}

SearchTermsDataSnapshot::~SearchTermsDataSnapshot() = default;

std::string SearchTermsDataSnapshot::GoogleBaseURLValue() const {
  return google_base_url_value_;
}

std::string SearchTermsDataSnapshot::GetApplicationLocale() const {
  return application_locale_;
}

std::u16string SearchTermsDataSnapshot::GetRlzParameterValue(
    bool from_app_list) const {
  return rlz_parameter_value_;
}

std::string SearchTermsDataSnapshot::GetSearchClient() const {
  return search_client_;
}

std::string SearchTermsDataSnapshot::GoogleImageSearchSource() const {
  return google_image_search_source_;
}

size_t SearchTermsDataSnapshot::EstimateMemoryUsage() const {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(google_base_url_value_);
  res += base::trace_event::EstimateMemoryUsage(application_locale_);
  res += base::trace_event::EstimateMemoryUsage(rlz_parameter_value_);
  res += base::trace_event::EstimateMemoryUsage(search_client_);
  res += base::trace_event::EstimateMemoryUsage(google_image_search_source_);

  return res;
}

}  // namespace

// static
std::unique_ptr<SearchTermsData> SearchTermsData::MakeSnapshot(
    const SearchTermsData* original_data) {
  return std::make_unique<SearchTermsDataSnapshot>(original_data);
}

SearchTermsData::SearchTermsData() = default;

SearchTermsData::~SearchTermsData() = default;

std::string SearchTermsData::GoogleBaseURLValue() const {
  return google_util::kGoogleHomepageURL;
}

std::string SearchTermsData::GoogleBaseSearchByImageURLValue() const {
  const std::string kGoogleHomepageURLPath = std::string("searchbyimage/");

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(lens::features::kLensStandalone)) {
    return lens::features::GetHomepageURLForLens();
  }
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

  return google_util::kGoogleHomepageURL + kGoogleHomepageURLPath;
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

std::u16string SearchTermsData::GetRlzParameterValue(bool from_app_list) const {
  return std::u16string();
}

std::string SearchTermsData::GetSearchClient() const {
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
