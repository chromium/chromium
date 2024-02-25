// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/page_features.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace dom_distiller {
namespace {

struct SiteDerivedFeatures {
  std::string url;
  std::vector<double> derived_features;
};

// Returns std::nullopt in case of failure.
std::optional<SiteDerivedFeatures> ParseSiteDerivedFeatures(
    const base::Value& json) {
  if (!json.is_dict())
    return std::nullopt;

  const std::string* url = json.GetDict().FindString("url");
  if (!url)
    return std::nullopt;

  const base::Value::List* derived_features_json =
      json.GetDict().FindList("features");
  if (!derived_features_json)
    return std::nullopt;

  // For every feature name at index 2*N, their value is at 2*N+1. In particular
  // the size must be an even number.
  size_t size = derived_features_json->size();
  if (size % 2 != 0)
    return std::nullopt;

  std::vector<double> derived_features;
  for (size_t i = 1; i < size; i += 2) {
    const base::Value& feature_value = (*derived_features_json)[i];
    // If the value is a bool, convert it to 1.0 or 0.0.
    double numerical_feature_value;
    if (feature_value.is_double() || feature_value.is_int())
      numerical_feature_value = feature_value.GetDouble();
    else if (feature_value.is_bool())
      numerical_feature_value = feature_value.GetBool();
    else
      return std::nullopt;

    derived_features.push_back(numerical_feature_value);
  }

  return SiteDerivedFeatures{*url, derived_features};
}

// Reads a JSON "{[....]}" into a base::Value::List. Returns an empty
// list in case of failure.
base::Value::List ReadJsonList(const std::string& file_name) {
  base::FilePath dir_src_test_data_root;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                              &dir_src_test_data_root)) {
    return base::Value::List();
  }

  std::string raw_content;
  bool read_success = base::ReadFileToString(
      dir_src_test_data_root.AppendASCII(file_name), &raw_content);
  if (!read_success)
    return base::Value::List();

  std::optional<base::Value> parsed_content =
      base::JSONReader::Read(raw_content);
  if (!parsed_content)
    return base::Value::List();

  if (!parsed_content->is_list())
    return base::Value::List();

  return std::move(*parsed_content).TakeList();
}

// This test uses input data of core features and the output of the training
// pipeline's derived feature extraction to ensure that the extraction that is
// done in Chromium matches that in the training pipeline.
TEST(DomDistillerPageFeaturesTest, TestCalculateDerivedFeatures) {
  // Read the expectations file into a format convenient for testing.
  const std::string kExpectationsFile =
      "components/test/data/dom_distiller/derived_features.json";
  std::vector<SiteDerivedFeatures> expected_sites_feature_info;
  for (const base::Value& site_info : ReadJsonList(kExpectationsFile)) {
    std::optional<SiteDerivedFeatures> parsed =
        ParseSiteDerivedFeatures(site_info);
    ASSERT_TRUE(parsed) << "Invalid expectation file";
    expected_sites_feature_info.push_back(*parsed);
  }

  base::Value::List input_sites_feature_info =
      ReadJsonList("components/test/data/dom_distiller/core_features.json");
  ASSERT_FALSE(input_sites_feature_info.empty());
  ASSERT_EQ(expected_sites_feature_info.size(),
            input_sites_feature_info.size());

  // Loop over |input_sites_feature_info| and |expected_sites_feature_info|
  // simultaneously, compute the derived features and verify they match the
  // expectation.
  auto input_iter = input_sites_feature_info.begin();
  auto expectation_iter = expected_sites_feature_info.begin();
  for (; input_iter != input_sites_feature_info.end();
       input_iter++, expectation_iter++) {
    ASSERT_TRUE(input_iter->is_dict());
    const auto& input_iter_dict = input_iter->GetDict();
    ASSERT_TRUE(input_iter_dict.FindString("url"));
    ASSERT_TRUE(input_iter_dict.FindDict("features"));
    // The JSON must be stringified for CalculateDerivedFeaturesFromJSON().
    base::Value stringified_json(base::Value::Type::STRING);
    bool success = base::JSONWriter::Write(
        *input_iter_dict.FindDict("features"), &stringified_json.GetString());
    ASSERT_TRUE(success);

    // Check the URL and the vector of features match the expectation.
    EXPECT_EQ(expectation_iter->url, *input_iter_dict.FindString("url"));
    EXPECT_EQ(expectation_iter->derived_features,
              CalculateDerivedFeaturesFromJSON(&stringified_json));
  }
}

std::vector<double> DeriveFromPath(const GURL& url) {
  return CalculateDerivedFeatures(false,  // bool openGraph
                                  url,    // const GURL& url
                                  0,      // unsigned elementCount
                                  0,      // unsigned anchorCount
                                  0,      // unsigned formCount
                                  0,      // double mozScore
                                  0,      // double mozScoreAllSqrt
                                  0       // double mozScoreAllLinear
  );
}

TEST(DomDistillerPageFeaturesTest, TestPath) {
  GURL url("http://example.com/search/view/index/the-title-of-archive.php");

  std::vector<double> derived(DeriveFromPath(url));
  EXPECT_EQ(0, lround(derived[1]));
  EXPECT_EQ(1, lround(derived[2]));
  EXPECT_EQ(1, lround(derived[3]));
  EXPECT_EQ(1, lround(derived[4]));
  EXPECT_EQ(1, lround(derived[5]));
  EXPECT_EQ(0, lround(derived[6]));
  EXPECT_EQ(0, lround(derived[7]));
  EXPECT_EQ(1, lround(derived[8]));
  EXPECT_EQ(43, lround(derived[9]));
  EXPECT_EQ(0, lround(derived[10]));
  EXPECT_EQ(4, lround(derived[11]));
  EXPECT_EQ(4, lround(derived[12]));
  EXPECT_EQ(0, lround(derived[13]));
  EXPECT_EQ(24, lround(derived[14]));
}

TEST(DomDistillerPageFeaturesTest, TestPath2) {
  GURL url("http://example.com/phpbb/forum123/456.asp");

  std::vector<double> derived(DeriveFromPath(url));
  EXPECT_EQ(1, lround(derived[1]));
  EXPECT_EQ(0, lround(derived[2]));
  EXPECT_EQ(0, lround(derived[3]));
  EXPECT_EQ(0, lround(derived[4]));
  EXPECT_EQ(0, lround(derived[5]));
  EXPECT_EQ(1, lround(derived[6]));
  EXPECT_EQ(1, lround(derived[7]));
  EXPECT_EQ(0, lround(derived[8]));
  EXPECT_EQ(23, lround(derived[9]));
  EXPECT_EQ(0, lround(derived[10]));
  EXPECT_EQ(3, lround(derived[11]));
  EXPECT_EQ(1, lround(derived[12]));
  EXPECT_EQ(2, lround(derived[13]));
  EXPECT_EQ(7, lround(derived[14]));
}

TEST(DomDistillerPageFeaturesTest, TestPath3) {
  GURL url("https://example.com/");

  std::vector<double> derived(DeriveFromPath(url));
  EXPECT_EQ(0, lround(derived[1]));
  EXPECT_EQ(0, lround(derived[2]));
  EXPECT_EQ(0, lround(derived[3]));
  EXPECT_EQ(0, lround(derived[4]));
  EXPECT_EQ(0, lround(derived[5]));
  EXPECT_EQ(0, lround(derived[6]));
  EXPECT_EQ(0, lround(derived[7]));
  EXPECT_EQ(0, lround(derived[8]));
  EXPECT_EQ(1, lround(derived[9]));
  EXPECT_EQ(1, lround(derived[10]));
  EXPECT_EQ(0, lround(derived[11]));
  EXPECT_EQ(0, lround(derived[12]));
  EXPECT_EQ(0, lround(derived[13]));
  EXPECT_EQ(1, lround(derived[14]));
}

TEST(DomDistillerPageFeaturesTest, TestPath4) {
  GURL url("https://example.com/trailing/");

  std::vector<double> derived(DeriveFromPath(url));
  EXPECT_EQ(0, lround(derived[1]));
  EXPECT_EQ(0, lround(derived[2]));
  EXPECT_EQ(0, lround(derived[3]));
  EXPECT_EQ(0, lround(derived[4]));
  EXPECT_EQ(0, lround(derived[5]));
  EXPECT_EQ(0, lround(derived[6]));
  EXPECT_EQ(0, lround(derived[7]));
  EXPECT_EQ(0, lround(derived[8]));
  EXPECT_EQ(10, lround(derived[9]));
  EXPECT_EQ(0, lround(derived[10]));
  EXPECT_EQ(1, lround(derived[11]));
  EXPECT_EQ(0, lround(derived[12]));
  EXPECT_EQ(0, lround(derived[13]));
  EXPECT_EQ(9, lround(derived[14]));
}

}  // namespace
}  // namespace dom_distiller
