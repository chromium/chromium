// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/history_fuzzy_provider.h"

#include <vector>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
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
  typename Container::iterator it = base::ranges::find(container, item);
  if (it == container.end()) {
    return;
  }
  typename Container::iterator last = container.end() - 1;
  if (it != last) {
    std::iter_swap(it, last);
  }
  container.pop_back();
}

// These operator implementations are for debugging.
std::ostream& operator<<(std::ostream& os, const fuzzy::Edit& edit) {
  os << '{';
  switch (edit.kind) {
    case fuzzy::Edit::Kind::KEEP: {
      os << 'K';
      break;
    }
    case fuzzy::Edit::Kind::DELETE: {
      os << 'D';
      break;
    }
    case fuzzy::Edit::Kind::INSERT: {
      os << 'I';
      break;
    }
    case fuzzy::Edit::Kind::REPLACE: {
      os << 'R';
      break;
    }
    case fuzzy::Edit::Kind::TRANSPOSE: {
      os << 'T';
      break;
    }
    default: {
      NOTREACHED_IN_MIGRATION();
      break;
    }
  }
  os << "," << edit.at << "," << static_cast<char>(edit.new_char) << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const fuzzy::Correction& correction) {
  os << '[';
  for (size_t i = 0; i < correction.edit_count; i++) {
    os << correction.edits[i];
    os << " <- ";
  }
  os << ']';
  return os;
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
    DVLOG(1) << correction << " -> " << corrected_input;
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

// This test ensures that transposition swaps two adjacent characters with
// a single operation at edit distance one. Only directly adjacent characters
// can be transposed and nonadjacent character swaps still require two edits.
TEST_F(HistoryFuzzyProviderTest, TransposeIsEditDistanceOne) {
  fuzzy::Node node;
  node.Insert(u"transpose", 0);

  std::vector<TestCase> cases = {
      {
          // Direct transposition 'op' -> 'po'. Finding the correction
          // with tolerance 1 implies a single transposition edit was enough.
          1,
          u"transopse",
          false,
          {
              u"transpose",
          },
      },
      {
          // Not a direct transposition, as the 's' is in between,
          // so this case requires insert + delete pair (tolerance 2).
          2,
          u"transpeso",
          false,
          {
              u"transpose",
          },
      },
  };

  VerifyCases(&node, cases);
}

// This test covers a subtlety in the algorithm. It ensures we don't take
// replacements at the same position of a previous insertion because we
// only want one of {I,1,e}~{R,0,t} (ler ter) | {R,0,e}~{I,0,t} (er ter).
TEST_F(HistoryFuzzyProviderTest, DoesNotProduceDuplicate) {
  fuzzy::Node node;
  node.Insert(u"ter", 0);

  std::vector<TestCase> cases = {
      {
          2,
          u"lr",
          false,
          {
              u"ter",
          },
      },
  };

  VerifyCases(&node, cases);
}

TEST_F(HistoryFuzzyProviderTest, NodesDeleteAndPreserveStructure) {
  fuzzy::Node node;
  const auto checked_delete = [&](const std::u16string& s) {
    node.Delete(s, 0);
    return node.TerminalCount() == 0;
  };
  node.Insert(u"abc", 0);
  CHECK(checked_delete(u"abc"));
  CHECK(node.next.empty());
  node.Insert(u"def", 0);
  CHECK(!checked_delete(u"de"));
  CHECK(!node.next.empty());
  CHECK(checked_delete(u"def"));
  node.Insert(u"ghi", 0);
  node.Insert(u"ghost", 0);
  CHECK(!checked_delete(u"gh"));
  CHECK(!node.next.empty());
  CHECK(!checked_delete(u"ghi"));
  CHECK(checked_delete(u"ghost"));
  CHECK(node.next.empty());
}

TEST_F(HistoryFuzzyProviderTest, NodesMaintainRelevanceTotalTerminalCount) {
  fuzzy::Node node;

  // Start with no terminals.
  CHECK_EQ(node.TerminalCount(), 0);

  // Support including the empty string as terminal.
  node.Insert(u"", 0);
  CHECK_EQ(node.TerminalCount(), 1);

  // Ensure repeated insertions don't cause growth.
  node.Insert(u"abc", 0);
  CHECK_EQ(node.TerminalCount(), 2);
  node.Insert(u"abc", 0);
  CHECK_EQ(node.TerminalCount(), 2);

  // Check further additions on different, same, and partially same paths.
  node.Insert(u"def", 0);
  CHECK_EQ(node.TerminalCount(), 3);
  node.Insert(u"abcd", 0);
  CHECK_EQ(node.TerminalCount(), 4);
  node.Insert(u"ab", 0);
  CHECK_EQ(node.next[u'a']->TerminalCount(), 3);
  CHECK_EQ(node.next[u'd']->TerminalCount(), 1);
  CHECK_EQ(node.TerminalCount(), 5);

  // Check deletion, including no-op and empty string deletion.
  node.Delete(u"a", 0);
  CHECK_EQ(node.TerminalCount(), 5);
  node.Delete(u"x", 0);
  CHECK_EQ(node.TerminalCount(), 5);
  node.Delete(u"", 0);
  CHECK_EQ(node.TerminalCount(), 4);
  node.Delete(u"abc", 0);
  CHECK_EQ(node.TerminalCount(), 3);
  node.Delete(u"abcd", 0);
  CHECK_EQ(node.TerminalCount(), 2);
  node.Delete(u"ab", 0);
  CHECK_EQ(node.TerminalCount(), 1);
  node.Delete(u"defx", 0);
  CHECK_EQ(node.TerminalCount(), 1);
  node.Delete(u"def", 0);
  CHECK_EQ(node.TerminalCount(), 0);
}
