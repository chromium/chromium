// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/language/content/browser/ulp_language_code_locator/s2langquadtree.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/s2cellid/src/s2/s2cellid.h"
#include "third_party/s2cellid/src/s2/s2latlng.h"

namespace language {
namespace {
#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator_helper.h"
}  // namespace

const std::map<S2LatLng, std::string> GetData(int rank) {
  std::map<S2LatLng, std::string> latlng_to_lang;

  std::string data;
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_dir));
  base::FilePath data_dir =
      source_dir.AppendASCII("components/test/data/language/");
  base::FilePath data_filepath = data_dir.AppendASCII(
      "celltolang-data_rank" + std::to_string(rank) + ".csv");

  if (!base::ReadFileToString(data_filepath, &data))
    LOG(FATAL) << "Could not read data from `" << data_filepath << "`.";
  std::vector<std::string> lines = base::SplitString(
      data, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t i = 0; i < lines.size(); ++i) {
    std::vector<std::string> fields = base::SplitString(
        lines[i], ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    CHECK_EQ(3u, fields.size());
    double lat, lng;
    CHECK(base::StringToDouble(fields[0], &lat));
    CHECK(base::StringToDouble(fields[1], &lng));
    latlng_to_lang[S2LatLng::FromDegrees(lat, lng)] = fields[2];
  }
  return latlng_to_lang;
}

void ExpectTreeContainsData(const S2LangQuadTreeNode& root,
                            const std::map<S2LatLng, std::string>& data) {
  int face = -1;
  for (const auto& latlng_lang : data) {
    S2CellId cell(latlng_lang.first);

    // All data is not on the same face, tree will fail.
    if (face != -1)
      EXPECT_EQ(face, cell.face());
    face = cell.face();

    EXPECT_EQ(latlng_lang.second, root.Get(cell));
  }
}

TEST(S2LangQuadTreeDataTest, TreeContainsDataRank0) {
  const BitsetSerializedLanguageTree<kNumBits0> serialized_langtree(
      GetLanguagesRank0(), GetTreeSerializedRank0());
  ExpectTreeContainsData(S2LangQuadTreeNode::Deserialize(&serialized_langtree),
                         GetData(0));
}

TEST(S2LangQuadTreeDataTest, TreeContainsDataRank1) {
  const BitsetSerializedLanguageTree<kNumBits1> serialized_langtree(
      GetLanguagesRank1(), GetTreeSerializedRank1());
  ExpectTreeContainsData(S2LangQuadTreeNode::Deserialize(&serialized_langtree),
                         GetData(1));
}

TEST(S2LangQuadTreeDataTest, TreeContainsDataRank2) {
  const BitsetSerializedLanguageTree<kNumBits2> serialized_langtree(
      GetLanguagesRank2(), GetTreeSerializedRank2());
  ExpectTreeContainsData(S2LangQuadTreeNode::Deserialize(&serialized_langtree),
                         GetData(2));
}

}  // namespace language
