// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TESTING_SEARCH_TERMS_DATA_H_
#define COMPONENTS_SEARCH_ENGINES_TESTING_SEARCH_TERMS_DATA_H_

#include "components/search_engines/search_terms_data.h"

class TestingSearchTermsData : public SearchTermsData {
 public:
  explicit TestingSearchTermsData(const std::string& google_base_url);

  TestingSearchTermsData(const TestingSearchTermsData&) = delete;
  TestingSearchTermsData& operator=(const TestingSearchTermsData&) = delete;

  ~TestingSearchTermsData() override;

  std::string GoogleBaseURLValue() const override;
  std::u16string GetRlzParameterValue(bool from_app_list) const override;
  std::string GetSearchClient() const override;
  std::string GoogleImageSearchSource() const override;

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const override;

  void set_google_base_url(const std::string& google_base_url) {
    google_base_url_ = google_base_url;
  }
  void set_search_client(const std::string& search_client) {
    search_client_ = search_client;
  }

 private:
  std::string google_base_url_;
  std::string search_client_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_TESTING_SEARCH_TERMS_DATA_H_
