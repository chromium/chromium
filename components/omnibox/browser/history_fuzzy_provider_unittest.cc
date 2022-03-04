// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_fuzzy_provider.h"

#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

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

}  // namespace

class HistoryFuzzyProviderTest : public testing::Test {
 public:
  HistoryFuzzyProviderTest() = default;
  HistoryFuzzyProviderTest(const HistoryFuzzyProviderTest&) = delete;
  HistoryFuzzyProviderTest& operator=(const HistoryFuzzyProviderTest&) = delete;

  void SetUp() override {}
};

TEST_F(HistoryFuzzyProviderTest, TrieProducesCorrections) {
  // TODO(orinj): Confirm that changing from memory store to table store
  //  doesn't break.
  fuzzy::Node node;

  node.Insert(u"abcdefg", 0);
  node.Insert(u"abcdxyz", 0);
  node.Insert(u"tuvwxyz", 0);
  node.Insert(u"tuvabcd", 0);

  struct Case {
    int tolerance;
    std::u16string input;
    bool expect_found;
    std::vector<std::u16string> corrected_inputs;
  };

  // A few things to note about these cases:
  // They don't complete to full strings; minimal corrections are supplied.
  // Tolerance consumption is currently naive. This should likely change,
  // as we may want to start with low (or even no) tolerance and then allow
  // incremental tolerance gains as more of the input string is scanned.
  // A stepping tolerance schedule like this would greatly increase efficiency
  // and allow more tolerance for longer strings without risking odd matches.
  Case cases[] = {
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
          10,
          u"abc____",
          false,
          {
              u"abcdefg",
              u"abcdxyz",
          },
      },
  };

  // Note: Each case is destroyed in place as it is checked.
  for (Case& test_case : cases) {
    std::vector<fuzzy::Correction> corrections;
    bool found = node.FindCorrections(test_case.input, 0, test_case.tolerance,
                                      corrections);
    CHECK_EQ(found, test_case.expect_found);
    CHECK_EQ(test_case.corrected_inputs.size(), corrections.size());
    for (const fuzzy::Correction& correction : corrections) {
      std::u16string corrected_input = test_case.input;
      correction.ApplyTo(corrected_input);
      SwapRemoveElement(test_case.corrected_inputs, corrected_input);
    }
    CHECK_EQ(test_case.corrected_inputs.size(), size_t{0});
  }
}
