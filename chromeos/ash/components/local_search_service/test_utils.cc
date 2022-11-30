// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/test_utils.h"

#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ash::local_search_service {

namespace {

// (content-id, content).
using ContentWithId = std::pair<std::string, std::string>;

// (content-id, content, weight).
using WeightedContentWithId = std::tuple<std::string, std::string, float>;

}  // namespace

std::vector<Data> CreateTestData(
    const std::map<std::string, std::vector<ContentWithId>>& input) {
  std::vector<Data> output;
  for (const auto& item : input) {
    Data data;
    data.id = item.first;
    // Hardcode to "en" because it's unclear what config locale will be when
    // running a test.
    // TODO(jiameng): allow locale to be passed in if there's a need to use
    // non-en data in tests.
    data.locale = "en";
    std::vector<Content>& contents = data.contents;
    for (const auto& content_with_id : item.second) {
      const Content content(content_with_id.first,
                            base::UTF8ToUTF16(content_with_id.second));
      contents.push_back(content);
    }
    output.push_back(data);
  }
  return output;
}

std::vector<Data> CreateTestData(
    const std::map<std::string,
                   std::vector<std::tuple<std::string, std::string, float>>>&
        input) {
  std::vector<Data> output;
  for (const auto& item : input) {
    Data data;
    data.id = item.first;
    // Hardcode to "en" because it's unclear what config locale will be when
    // running a test.
    // TODO(jiameng): allow locale to be passed in if there's a need to use
    // non-en data in tests.
    data.locale = "en";
    std::vector<Content>& contents = data.contents;
    for (const auto& weighted_content_with_id : item.second) {
      const Content content(
          std::get<0>(weighted_content_with_id),
          base::UTF8ToUTF16(std::get<1>(weighted_content_with_id)),
          std::get<2>(weighted_content_with_id));
      contents.push_back(content);
    }
    output.push_back(data);
  }
  return output;
}

void CheckResult(const Result& result,
                 const std::string& expected_id,
                 float expected_score,
                 size_t expected_number_positions) {
  EXPECT_EQ(result.id, expected_id);
  EXPECT_NEAR(result.score, expected_score, 0.001);
  EXPECT_EQ(result.positions.size(), expected_number_positions);
}

float TfIdfScore(size_t num_docs,
                 size_t num_docs_with_term,
                 float weighted_num_term_occurrence_in_doc,
                 size_t doc_length) {
  const float idf = 1.0 + log((1.0 + num_docs) / (1.0 + num_docs_with_term));

  const float tf = weighted_num_term_occurrence_in_doc / doc_length;
  return tf * idf;
}

}  // namespace ash::local_search_service
