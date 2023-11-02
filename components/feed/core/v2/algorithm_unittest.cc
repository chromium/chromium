// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/algorithm.h"

#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace feed {
class ResultAggregator {
 public:
  explicit ResultAggregator(std::vector<std::pair<int, int>>* output)
      : results_(output) {}

  void operator()(int* left, int* right) {
    results_->emplace_back(left ? *left : -1, right ? *right : -1);
  }

 private:
  raw_ptr<std::vector<std::pair<int, int>>> results_;
};

TEST(DiffSortedRange, LeftEmpty) {
  std::vector<int> left;
  std::vector<int> right = {1, 2, 3};
  std::vector<std::pair<int, int>> results;
  DiffSortedRange(left.begin(), left.end(), right.begin(), right.end(),
                  ResultAggregator(&results));

  EXPECT_EQ((std::vector<std::pair<int, int>>{{-1, 1}, {-1, 2}, {-1, 3}}),
            results);
}

TEST(DiffSortedRange, RightEmpty) {
  std::vector<int> left = {1, 2, 3};
  std::vector<int> right;
  std::vector<std::pair<int, int>> results;
  DiffSortedRange(left.begin(), left.end(), right.begin(), right.end(),
                  ResultAggregator(&results));

  EXPECT_EQ((std::vector<std::pair<int, int>>{{1, -1}, {2, -1}, {3, -1}}),
            results);
}

TEST(DiffSortedRange, EndsWithRight) {
  std::vector<int> left = {1, 2, 4};
  std::vector<int> right = {2, 3, 4, 5};
  std::vector<std::pair<int, int>> results;
  DiffSortedRange(left.begin(), left.end(), right.begin(), right.end(),
                  ResultAggregator(&results));

  EXPECT_EQ((std::vector<std::pair<int, int>>{
                {1, -1}, {2, 2}, {-1, 3}, {4, 4}, {-1, 5}}),
            results);
}

TEST(DiffSortedRange, EndsWithLeft) {
  std::vector<int> left = {2, 4, 5};
  std::vector<int> right = {1, 3, 4};
  std::vector<std::pair<int, int>> results;
  DiffSortedRange(left.begin(), left.end(), right.begin(), right.end(),
                  ResultAggregator(&results));

  EXPECT_EQ((std::vector<std::pair<int, int>>{
                {-1, 1}, {2, -1}, {-1, 3}, {4, 4}, {5, -1}}),
            results);
}

}  // namespace feed
