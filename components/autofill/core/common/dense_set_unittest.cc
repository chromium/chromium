// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/dense_set.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(DenseSet, initialization) {
  enum class T : size_t {
    One = 1,
    Two = 2,
    Three = 3,
    Four = 4,
    Five = 5,
    kEnd = 6
  };
  using DS = DenseSet<T, T::kEnd>;

  DS s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
  EXPECT_EQ(DS(s.begin(), s.end()), s);
  s.insert(T::Two);
  s.insert(T::Four);
  s.insert(T::One);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(DS(s.begin(), s.end()), s);
  EXPECT_EQ(DS(s.cbegin(), s.cend()), s);
  EXPECT_EQ(DS(s.rbegin(), s.rend()), s);
  EXPECT_EQ(DS(s.crbegin(), s.crend()), s);
  EXPECT_EQ(DS({T::Four, T::Two, T::One}), s);
}

TEST(DenseSet, iterators_begin_end) {
  enum class T : int {
    One = 1,
    Two = 2,
    Three = 3,
    Four = 4,
    Five = 5,
    kEnd = 6
  };
  using DS = DenseSet<T, T::kEnd>;

  DS s;
  s.insert(T::Two);
  s.insert(T::Four);
  s.insert(T::One);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(std::distance(s.begin(), s.end()), 3);

  {
    auto it = s.begin();
    auto x1 = *it++;
    auto x2 = *it++;
    auto x3 = *it++;
    EXPECT_EQ(it, s.end());
    EXPECT_EQ(x1, T::One);
    EXPECT_EQ(x2, T::Two);
    EXPECT_EQ(x3, T::Four);
  }

  {
    auto it = s.begin();
    auto x1 = *it;
    auto x2 = *++it;
    auto x3 = *++it;
    EXPECT_NE(it, s.end());
    EXPECT_EQ(x1, T::One);
    EXPECT_EQ(x2, T::Two);
    EXPECT_EQ(x3, T::Four);
  }

  EXPECT_THAT(s, ::testing::ElementsAre(T::One, T::Two, T::Four));
}

TEST(DenseSet, iterators_begin_end_reverse) {
  enum class T : char {
    One = 1,
    Two = 2,
    Three = 3,
    Four = 4,
    Five = 5,
    kEnd = 6
  };
  using DS = DenseSet<T, T::kEnd>;

  DS s;
  s.insert(T::Two);
  s.insert(T::Four);
  s.insert(T::One);
  EXPECT_EQ(s.size(), 3u);

  {
    auto it = s.end();
    it--;
    auto x3 = *it--;
    auto x2 = *it--;
    auto x1 = *it;
    EXPECT_EQ(it, s.begin());
    EXPECT_EQ(x1, T::One);
    EXPECT_EQ(x2, T::Two);
    EXPECT_EQ(x3, T::Four);
  }

  {
    auto it = s.end();
    auto x3 = *--it;
    auto x2 = *--it;
    auto x1 = *--it;
    EXPECT_EQ(it, s.begin());
    EXPECT_EQ(x1, T::One);
    EXPECT_EQ(x2, T::Two);
    EXPECT_EQ(x3, T::Four);
  }
}

TEST(DenseSet, iterators_rbegin_rend) {
  enum class T { One = 1, Two = 2, Three = 3, Four = 4, Five = 5, kEnd = 6 };
  using DS = DenseSet<T, T::kEnd>;

  DS s;
  s.insert(T::Two);
  s.insert(T::Four);
  s.insert(T::One);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(std::distance(s.rbegin(), s.rend()), 3);

  {
    auto it = s.rbegin();
    auto x3 = *it++;
    auto x2 = *it++;
    auto x1 = *it++;
    EXPECT_EQ(it, s.rend());
    EXPECT_EQ(x1, T::One);
    EXPECT_EQ(x2, T::Two);
    EXPECT_EQ(x3, T::Four);
  }

  {
    auto it = s.rbegin();
    auto x3 = *it;
    auto x2 = *++it;
    auto x1 = *++it;
    EXPECT_NE(it, s.rend());
    EXPECT_EQ(x1, T::One);
    EXPECT_EQ(x2, T::Two);
    EXPECT_EQ(x3, T::Four);
  }

  EXPECT_THAT(std::vector<T>(s.rbegin(), s.rend()),
              ::testing::ElementsAre(T::Four, T::Two, T::One));
}

TEST(DenseSet, lookup) {
  enum class T { One = 1, Two = 2, Three = 3, Four = 4, Five = 5, kEnd = 6 };
  using DS = DenseSet<T, T::kEnd>;

  DS s;
  s.insert(T::Two);
  s.insert(T::Four);
  s.insert(T::One);

  EXPECT_FALSE(s.contains(static_cast<T>(0)));
  EXPECT_TRUE(s.contains(T::One));
  EXPECT_TRUE(s.contains(T::Two));
  EXPECT_FALSE(s.contains(T::Three));
  EXPECT_TRUE(s.contains(T::Four));
  EXPECT_FALSE(s.contains(T::Five));

  EXPECT_EQ(s.contains(static_cast<T>(0)), 0u);
  EXPECT_EQ(s.contains(T::One), 1u);
  EXPECT_EQ(s.contains(T::Two), 1u);
  EXPECT_EQ(s.contains(T::Three), 0u);
  EXPECT_EQ(s.contains(T::Four), 1u);
  EXPECT_EQ(s.contains(T::Five), 0u);

  EXPECT_EQ(s.find(static_cast<T>(0)), s.end());
  EXPECT_NE(s.find(T::One), s.end());
  EXPECT_NE(s.find(T::Two), s.end());
  EXPECT_EQ(s.find(T::Three), s.end());
  EXPECT_NE(s.find(T::Four), s.end());
  EXPECT_EQ(s.find(T::Five), s.end());

  EXPECT_EQ(*s.find(T::One), T::One);
  EXPECT_EQ(*s.find(T::Two), T::Two);
  EXPECT_EQ(*s.find(T::Four), T::Four);

  EXPECT_NE(s.find(static_cast<T>(0)), s.lower_bound(static_cast<T>(0)));
  EXPECT_EQ(s.find(T::One), s.lower_bound(T::One));
  EXPECT_EQ(s.find(T::Two), s.lower_bound(T::Two));
  EXPECT_NE(s.find(T::Three), s.lower_bound(T::Three));
  EXPECT_EQ(s.find(T::Four), s.lower_bound(T::Four));
  EXPECT_EQ(s.find(T::Five), s.lower_bound(T::Five));
}

TEST(DenseSet, iterators_lower_upper_bound) {
  enum class T { One = 1, Two = 2, Three = 3, Four = 4, Five = 5, kEnd = 6 };
  using DS = DenseSet<T, T::kEnd>;

  DS s;
  s.insert(T::Two);
  s.insert(T::Four);
  s.insert(T::One);
  EXPECT_EQ(s.size(), 3u);

  EXPECT_EQ(s.lower_bound(static_cast<T>(0)), s.begin());
  EXPECT_EQ(s.lower_bound(T::One), s.begin());

  EXPECT_EQ(s.upper_bound(T::Four), s.end());
  EXPECT_EQ(s.upper_bound(T::Five), s.end());

  {
    auto it = s.lower_bound(static_cast<T>(0));
    auto jt = s.upper_bound(static_cast<T>(0));
    EXPECT_EQ(it, jt);
  }

  {
    auto it = s.lower_bound(T::One);
    auto jt = s.upper_bound(T::One);
    auto x1 = *it++;
    EXPECT_EQ(it, jt);
    EXPECT_EQ(x1, T::One);
  }

  {
    auto it = s.lower_bound(T::Four);
    auto jt = s.upper_bound(T::Four);
    auto x3 = *it++;
    EXPECT_EQ(it, jt);
    EXPECT_EQ(x3, T::Four);
  }

  {
    auto it = s.lower_bound(T::Five);
    auto jt = s.upper_bound(T::Five);
    EXPECT_EQ(it, jt);
  }

  {
    auto it = s.lower_bound(T::One);
    auto jt = s.upper_bound(T::Five);
    auto x1 = *it++;
    auto x2 = *it++;
    auto x3 = *it++;
    EXPECT_EQ(it, jt);
    EXPECT_EQ(x1, T::One);
    EXPECT_EQ(x2, T::Two);
    EXPECT_EQ(x3, T::Four);
  }

  {
    auto it = s.lower_bound(T::Three);
    auto jt = s.upper_bound(T::Four);
    auto x3 = *it++;
    EXPECT_EQ(jt, s.end());
    EXPECT_EQ(it, jt);
    EXPECT_EQ(x3, T::Four);
  }

  EXPECT_EQ(static_cast<size_t>(std::distance(s.begin(), s.end())), s.size());
  EXPECT_EQ(std::next(std::next(std::next(s.begin()))), s.end());
}

TEST(DenseSet, max_size) {
  const int One = 1;
  const int Two = 2;
  // const int Three = 3;
  const int Four = 4;
  // const int Five = 5;
  const int kEnd = 6;
  using DS = DenseSet<int, kEnd>;

  DS s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
  EXPECT_EQ(s.max_size(), 6u);
  s.insert(Two);
  EXPECT_FALSE(s.empty());
  EXPECT_EQ(s.size(), 1u);
  s.insert(Four);
  EXPECT_FALSE(s.empty());
  EXPECT_EQ(s.size(), 2u);
  s.insert(One);
  EXPECT_FALSE(s.empty());
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(s.max_size(), 6u);
}

TEST(DenseSet, modifiers) {
  const size_t One = 1;
  const size_t Two = 2;
  const size_t Three = 3;
  const size_t Four = 4;
  // const size_t Five = 5;
  const size_t kEnd = 6;
  using DS = DenseSet<size_t, kEnd>;

  DS s;
  s.insert(Two);
  s.insert(Four);
  s.insert(One);
  EXPECT_EQ(s.size(), 3u);

  auto EXPECT_INSERTION = [](auto& set, auto value, bool took_place) {
    auto it = set.insert(value);
    EXPECT_EQ(it, std::make_pair(set.find(value), took_place));
  };

  DS t;
  EXPECT_NE(s, t);
  EXPECT_INSERTION(t, Two, true);
  EXPECT_INSERTION(t, Two, false);
  EXPECT_INSERTION(t, Four, true);
  EXPECT_INSERTION(t, Four, false);
  EXPECT_INSERTION(t, One, true);
  EXPECT_INSERTION(t, One, false);
  EXPECT_EQ(s, t);
  EXPECT_EQ(t.size(), 3u);

  EXPECT_INSERTION(t, Three, true);
  EXPECT_INSERTION(t, Three, false);
  EXPECT_EQ(t.erase(Three), 1u);
  EXPECT_EQ(t.erase(Three), 0u);
  EXPECT_EQ(s, t);
  EXPECT_EQ(t.size(), 3u);

  EXPECT_EQ(s.erase(One), 1u);
  EXPECT_EQ(t.erase(Four), 1u);
  EXPECT_NE(s, t);
  EXPECT_EQ(s.size(), 2u);
  EXPECT_EQ(t.size(), 2u);

  EXPECT_INSERTION(s, One, true);
  EXPECT_INSERTION(t, Four, true);
  EXPECT_EQ(s, t);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(t.size(), 3u);

  EXPECT_EQ(s.erase(s.find(One)), s.find(Two));
  EXPECT_EQ(t.erase(t.lower_bound(One), t.upper_bound(One)), t.find(Two));
  EXPECT_FALSE(s.contains(One));
  EXPECT_EQ(s, t);
  EXPECT_EQ(s.size(), 2u);
  EXPECT_EQ(t.size(), 2u);

  EXPECT_INSERTION(s, One, true);
  EXPECT_INSERTION(t, One, true);
  EXPECT_EQ(s, t);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(t.size(), 3u);

  EXPECT_EQ(s.erase(s.find(Two), s.end()), s.end());
  EXPECT_EQ(t.erase(t.lower_bound(Two), t.upper_bound(Four)), t.end());
  EXPECT_TRUE(s.contains(One));
  EXPECT_EQ(s, t);
  EXPECT_EQ(s.size(), 1u);
  EXPECT_EQ(t.size(), 1u);

  EXPECT_INSERTION(s, Two, true);
  EXPECT_INSERTION(t, Two, true);
  EXPECT_INSERTION(s, Four, true);
  EXPECT_INSERTION(t, Four, true);
  EXPECT_EQ(s, t);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(t.size(), 3u);

  s.clear();
  EXPECT_EQ(s, DS());
  EXPECT_EQ(s.size(), 0u);

  EXPECT_INSERTION(s, *t.begin(), true);
  EXPECT_TRUE(s.contains(One));
  EXPECT_INSERTION(s, *std::next(t.begin()), true);
  EXPECT_TRUE(s.contains(Two));
  EXPECT_INSERTION(s, *std::prev(t.end()), true);
  EXPECT_TRUE(s.contains(Four));
  EXPECT_EQ(s, t);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(t.size(), 3u);
}

TEST(DenseSet, std_set) {
  constexpr size_t kEnd = 50;
  DenseSet<size_t, kEnd> dense_set;
  std::set<size_t> std_set;

  auto expect_equivalence = [&] {
    EXPECT_EQ(dense_set.empty(), std_set.empty());
    EXPECT_EQ(dense_set.size(), std_set.size());
    EXPECT_TRUE(base::ranges::equal(dense_set, std_set));
  };

  auto random_insert = [&] {
    expect_equivalence();
    size_t value = base::RandUint64() % kEnd;
    auto p = dense_set.insert(value);
    auto q = std_set.insert(value);
    EXPECT_EQ(p.second, q.second);
    EXPECT_EQ(p.first == dense_set.end(), q.first == std_set.end());
    EXPECT_TRUE(!p.second || p.first == dense_set.find(value));
    EXPECT_TRUE(!q.second || q.first == std_set.find(value));
  };

  auto random_erase = [&] {
    expect_equivalence();
    size_t value = base::RandUint64() % kEnd;
    EXPECT_EQ(dense_set.erase(value), std_set.erase(value));
  };

  auto random_erase_iterator = [&] {
    expect_equivalence();
    size_t value = base::RandUint64() % kEnd;
    auto it = dense_set.find(value);
    auto jt = std_set.find(value);
    EXPECT_EQ(it == dense_set.end(), jt == std_set.end());
    if (it == dense_set.end() || jt == std_set.end())
      return;
    auto succ_it = dense_set.erase(it);
    auto succ_jt = std_set.erase(jt);
    EXPECT_EQ(succ_it == dense_set.end(), succ_jt == std_set.end());
    EXPECT_TRUE(succ_it == dense_set.upper_bound(value));
    EXPECT_TRUE(succ_jt == std_set.upper_bound(value));
    EXPECT_TRUE(succ_it == dense_set.end() || *succ_it == *succ_jt);
  };

  auto random_erase_range = [&] {
    expect_equivalence();
    size_t min_value = base::RandUint64() % kEnd;
    size_t max_value = base::RandUint64() % kEnd;
    min_value = std::min(min_value, max_value);
    max_value = std::max(min_value, max_value);
    dense_set.erase(dense_set.lower_bound(min_value),
                    dense_set.upper_bound(max_value));
    std_set.erase(std_set.lower_bound(min_value),
                  std_set.upper_bound(max_value));
  };

  for (size_t i = 0; i < kEnd; ++i) {
    random_insert();
  }

  for (size_t i = 0; i < kEnd / 2; ++i) {
    random_erase();
  }

  expect_equivalence();
  dense_set.clear();
  std_set.clear();
  expect_equivalence();

  for (size_t i = 0; i < kEnd; ++i) {
    random_insert();
  }

  for (size_t i = 0; i < kEnd; ++i) {
    random_erase_iterator();
  }

  expect_equivalence();
  dense_set.clear();
  std_set.clear();
  expect_equivalence();

  for (size_t i = 0; i < kEnd; ++i) {
    random_insert();
  }

  for (size_t i = 0; i < kEnd; ++i) {
    random_erase_range();
  }

  expect_equivalence();
}

}  // namespace autofill
