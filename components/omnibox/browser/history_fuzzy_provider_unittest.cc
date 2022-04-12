// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_fuzzy_provider.h"

#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TestCase {
  int tolerance;
  std::u16string input;
  bool expect_found;
  std::vector<std::u16string> corrected_inputs;
};

template <typename Container, typename Item>
void SwapRemoveElement(Container& container, const Item& item) {
  typename Container::iterator it =
      std::find(container.begin(), container.end(), item);
  if (it == container.end()) {
    return;
  }
  typename Container::iterator last = container.end() - 1;
  if (it != last) {
    std::iter_swap(it, last);
  }
  container.pop_back();
}

// Note: The `test_case` is destroyed in place as it is checked.
void VerifyTestCase(fuzzy::Node* node,
                    TestCase& test_case,
                    fuzzy::ToleranceSchedule tolerance_schedule) {
  std::vector<fuzzy::Correction> corrections;
  bool found =
      node->FindCorrections(test_case.input, tolerance_schedule, corrections);
  for (const fuzzy::Correction& correction : corrections) {
    std::u16string corrected_input = test_case.input;
    correction.ApplyTo(corrected_input);
    DVLOG(1) << "-> " << corrected_input;
  }
  CHECK_EQ(found, test_case.expect_found)
      << " input(" << test_case.tolerance << "): " << test_case.input;
  CHECK_EQ(test_case.corrected_inputs.size(), corrections.size())
      << " input(" << test_case.tolerance << "): " << test_case.input;
  for (const fuzzy::Correction& correction : corrections) {
    std::u16string corrected_input = test_case.input;
    correction.ApplyTo(corrected_input);
    SwapRemoveElement(test_case.corrected_inputs, corrected_input);
  }
  CHECK_EQ(test_case.corrected_inputs.size(), size_t{0})
      << " input(" << test_case.tolerance << "): " << test_case.input;
}

// Verifies a vector of `cases`, destroying them in the process.
void VerifyCases(fuzzy::Node* node, std::vector<TestCase>& cases) {
  for (TestCase& test_case : cases) {
    VerifyTestCase(
        node, test_case,
        {.start_index = 0, .step_length = 0, .limit = test_case.tolerance});
  }
}

// This is just like `VerifyCases` but uses a specified `tolerance_schedule`
// for all cases instead of each TestCase `tolerance` value.
void VerifyCasesWithSchedule(fuzzy::Node* node,
                             std::vector<TestCase>& cases,
                             fuzzy::ToleranceSchedule tolerance_schedule) {
  for (TestCase& test_case : cases) {
    VerifyTestCase(node, test_case, tolerance_schedule);
  }
}

}  // namespace

class HistoryFuzzyProviderTest : public testing::Test {
 public:
  HistoryFuzzyProviderTest() = default;
  HistoryFuzzyProviderTest(const HistoryFuzzyProviderTest&) = delete;
  HistoryFuzzyProviderTest& operator=(const HistoryFuzzyProviderTest&) = delete;

  void SetUp() override {}
};

TEST_F(HistoryFuzzyProviderTest, AlgorithmIsNotGreedy) {
  fuzzy::Node node;
  node.Insert(u"wind", 0);
  node.Insert(u"wash", 0);

  std::vector<TestCase> cases = {
      {
          1,
          u"wand",
          false,
          {
              u"wind",
          },
      },
      {
          1,
          u"wish",
          false,
          {
              u"wash",
          },
      },
      {
          2,
          u"xasx",
          false,
          {
              u"wash",
          },
      },
      {
          3,
          u"xaxsx",
          false,
          {
              u"wash",
          },
      },
      {
          2,
          u"want",
          false,
          {
              u"wind",
              u"wash",
          },
      },
  };

  VerifyCases(&node, cases);
}

TEST_F(HistoryFuzzyProviderTest, ReplacementWorksAnywhere) {
  fuzzy::Node node;
  node.Insert(u"abcdefg", 0);
  node.Insert(u"abcdxyz", 0);
  node.Insert(u"tuvwxyz", 0);
  node.Insert(u"tuvabcd", 0);

  // A few things to note about these cases:
  // They don't complete to full strings; minimal corrections are supplied.
  // Tolerance consumption is currently naive. This should likely change,
  // as we may want to start with low (or even no) tolerance and then allow
  // incremental tolerance gains as more of the input string is scanned.
  // A stepping tolerance schedule like this would greatly increase efficiency
  // and allow more tolerance for longer strings without risking odd matches.
  std::vector<TestCase> cases = {
      {
          0,
          u"abcdefg",
          true,
          {},
      },
      {
          0,
          u"abc_efg",
          false,
          {},
      },
      {
          1,
          u"abc_efg",
          false,
          {
              u"abcdefg",
          },
      },
      {
          1,
          u"abc_ef",
          false,
          {
              u"abcdef",
          },
      },
      {
          2,
          u"abc_e_g",
          false,
          {
              u"abcdefg",
          },
      },
      {
          2,
          u"a_c_e_g",
          false,
          {},
      },
      {
          3,
          u"a_c_e_",
          false,
          {
              u"abcdef",
          },
      },
      {
          4,
          u"____xyz",
          false,
          {
              u"abcdxyz",
              u"tuvwxyz",
          },
      },
      {
          4,
          u"abc____",
          false,
          {
              u"abcdefg",
              u"abcdxyz",
          },
      },
  };

  VerifyCases(&node, cases);
}

TEST_F(HistoryFuzzyProviderTest, InsertionWorksAnywhereExceptEnd) {
  fuzzy::Node node;
  node.Insert(u"abc", 0);

  std::vector<TestCase> cases = {
      {
          1,
          u"bc",
          false,
          {
              u"abc",
          },
      },
      {
          1,
          u"ac",
          false,
          {
              u"abc",
          },
      },
      {
          1,
          u"ab",
          true,
          {
              // Note, we are NOT expecting "abc" here because insertion at
              // end of input is generally predictive, and that is the job
              // of the whole autocomplete system, not the fuzzy input
              // correction algorithm, which seeks only to get inputs back
              // on track for good suggestions.
          },
      },
      {
          2,
          u"b",
          false,
          {
              u"ab",
          },
      },
  };

  VerifyCases(&node, cases);
}

TEST_F(HistoryFuzzyProviderTest, DeletionWorksAnywhere) {
  fuzzy::Node node;
  node.Insert(u"abc", 0);

  std::vector<TestCase> cases = {
      {
          1,
          u"xabc",
          false,
          {
              u"abc",
          },
      },
      {
          1,
          u"abxc",
          false,
          {
              u"abc",
          },
      },
      {
          1,
          u"abcx",
          false,
          {
              u"abc",
          },
      },
      {
          2,
          u"axbxc",
          false,
          {
              u"abc",
          },
      },
  };

  VerifyCases(&node, cases);
}

// This test ensures a preference for longer results when edit distances are
// equal. This isn't an absolute requirement, and some relevance or probability
// guidance might be better, but this simple heuristic avoids creating shorter
// substring corrections, for example both "was" and "wash".
TEST_F(HistoryFuzzyProviderTest, LongerResultsArePreferred) {
  fuzzy::Node node;
  node.Insert(u"ao", 0);
  node.Insert(u"eeo", 0);

  std::vector<TestCase> cases = {
      {
          1,
          u"eo",
          false,
          {
              u"eeo",
          },
      },
  };

  VerifyCases(&node, cases);
}

TEST_F(HistoryFuzzyProviderTest, EmptyTrieRespectsToleranceSchedule) {
  fuzzy::Node node;
  TestCase test_case;

  // Blank is produced by deleting at index zero.
  test_case = {
      0,
      u"x",
      false,
      {
          u"",
      },
  };
  VerifyTestCase(&node, test_case, {.start_index = 0, .limit = 1});

  // But this is not allowed when `start_index` is one.
  test_case = {
      0,
      u"x",
      false,
      {},
  };
  VerifyTestCase(&node, test_case, {.start_index = 1, .limit = 1});
}

TEST_F(HistoryFuzzyProviderTest, ToleranceScheduleIsEnforced) {
  fuzzy::Node node;
  node.Insert(u"abcdefghijklmnopqrstuv", 0);

  std::vector<TestCase> cases = {
      {
          0,
          u"axcdef",
          false,
          {},
      },
      {
          0,
          u"abxdef",
          false,
          {
              u"abcdef",
          },
      },
      {
          0,
          u"abxxdef",
          false,
          {},
      },
      {
          0,
          u"abxdexghi",
          false,
          {},
      },
      {
          0,
          u"abxdefxhi",
          false,
          {
              u"abcdefghi",
          },
      },
      {
          0,
          u"abxdefxhijxlmnop",
          false,
          {
              u"abcdefghijklmnop",
          },
      },
      {
          0,
          u"abxdefxhijxlmnopqx",
          false,
          {},
      },
  };

  VerifyCasesWithSchedule(&node, cases,
                          {.start_index = 2, .step_length = 4, .limit = 3});
}
