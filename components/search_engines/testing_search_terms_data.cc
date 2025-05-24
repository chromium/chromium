// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/testing_search_terms_data.h"

#include "base/strings/utf_string_conversions.h"

TestingSearchTermsData::TestingSearchTermsData(
    const std::string& google_base_url)
    : google_base_url_(google_base_url) {
}

TestingSearchTermsData::~TestingSearchTermsData() = default;

std::string TestingSearchTermsData::GoogleBaseURLValue() const {
  return google_base_url_;
}

std::u16string TestingSearchTermsData::GetRlzParameterValue(
    bool from_app_list) const {
  return base::ASCIIToUTF16(
      from_app_list ? "rlz_parameter_from_app_list" : "rlz_parameter");
}

std::string TestingSearchTermsData::GetSearchClient() const {
  return search_client_;
}

std::string TestingSearchTermsData::GoogleImageSearchSource() const {
  return "google_image_search_source";
}

size_t TestingSearchTermsData::EstimateMemoryUsage() const {
  return 0;
}
