// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_fuzzy_provider.h"

#include "testing/gtest/include/gtest/gtest.h"

class HistoryFuzzyProviderTest : public testing::Test {
 public:
  HistoryFuzzyProviderTest() = default;
  HistoryFuzzyProviderTest(const HistoryFuzzyProviderTest&) = delete;
  HistoryFuzzyProviderTest& operator=(const HistoryFuzzyProviderTest&) = delete;

  void SetUp() override {}
};

TEST_F(HistoryFuzzyProviderTest, TestRuns) {}
