// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/dense_set.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace autofill {

TEST(DenseSetTest, size_of) {
  EXPECT_EQ(sizeof(DenseSet<size_t, 0, 1>), 1u);
  EXPECT_EQ(sizeof(DenseSet<size_t, 0, 7>), 1u);
  EXPECT_EQ(sizeof(DenseSet<size_t, 0, 8>), 2u);
  EXPECT_EQ(sizeof(DenseSet<size_t, 0, 15>), 2u);
  EXPECT_EQ(sizeof(DenseSet<size_t, 0, 16>), 4u);
  EXPECT_EQ(sizeof(DenseSet<size_t, 0, 31>), 4u);
  EXPECT_EQ(sizeof(DenseSet<size_t, 0, 32>), 8u);
  EXPECT_EQ(sizeof(DenseSet<size_t, 0, 63>), 8u);
  EXPECT_EQ(sizeof(DenseSet<size_t, 0, 64>), 16u);
  EXPECT_EQ(sizeof(DenseSet<size_t, 0, 127>), 16u);
  EXPECT_EQ(sizeof(DenseSet<size_t, 0, 255>), 32u);
}

TEST(DenseSetTest, initialization) {
  enum class T : size_t {
    kOne = 1,
    kTwo = 2,
    kThree = 3,
    kFour = 4,
    kFive = 5,
    kMaxValue = kFive,
  };
  using DS = DenseSet<T>;

  DS s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
  EXPECT_EQ(DS(s.begin(), s.end()), s);
  s.insert(T::kTwo);
  s.insert(T::kFour);
  s.insert(T::kOne);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(DS(s.begin(), s.end()), s);
  EXPECT_EQ(DS(s.cbegin(), s.cend()), s);
  EXPECT_EQ(DS(s.rbegin(), s.rend()), s);
  EXPECT_EQ(DS(s.crbegin(), s.crend()), s);
  EXPECT_EQ(DS({T::kFour, T::kTwo, T::kOne}), s);
}

TEST(DenseSetTest, initializer_list) {
  // The largest value so that DenseSet offers a constexpr constructor.
  constexpr size_t kMaxValueForConstexpr = 63;

  // Each of the below blocks is a copy that only varies in `kMax` and whether
  // or not the `set` is `constexpr`.

  {
    constexpr size_t kMax = 10;
    constexpr DenseSet<size_t, 0, kMax> set{0, 1, kMax - 2, kMax - 1, kMax};
    EXPECT_THAT(std::vector<size_t>(set.begin(), set.end()),
                ::testing::ElementsAre(0, 1, kMax - 2, kMax - 1, kMax));
  }

  {
    constexpr size_t kMax = kMaxValueForConstexpr;
    constexpr DenseSet<size_t, 0, kMax> set{0, 1, kMax - 2, kMax - 1, kMax};
    EXPECT_THAT(std::vector<size_t>(set.begin(), set.end()),
                ::testing::ElementsAre(0, 1, kMax - 2, kMax - 1, kMax));
  }

  {
    constexpr size_t kMax = kMaxValueForConstexpr + 1;
    DenseSet<size_t, 0, kMax> set{0, 1, kMax - 2, kMax - 1, kMax};
    EXPECT_THAT(std::vector<size_t>(set.begin(), set.end()),
                ::testing::ElementsAre(0, 1, kMax - 2, kMax - 1, kMax));
  }

  {
    constexpr size_t kMax = kMaxValueForConstexpr + 2;
    DenseSet<size_t, 0, kMax> set{0, 1, kMax - 2, kMax - 1, kMax};
    EXPECT_THAT(std::vector<size_t>(set.begin(), set.end()),
                ::testing::ElementsAre(0, 1, kMax - 2, kMax - 1, kMax));
  }

  {
    constexpr size_t kMax = kMaxValueForConstexpr + 100;
    DenseSet<size_t, 0, kMax> set{0, 1, kMax - 2, kMax - 1, kMax};
    EXPECT_THAT(std::vector<size_t>(set.begin(), set.end()),
                ::testing::ElementsAre(0, 1, kMax - 2, kMax - 1, kMax));
  }
}

TEST(DenseSetTest, data) {
  {
    constexpr DenseSet<size_t, 0, 23> set{0, 1, 2, 3, 4, 20, 23};
    EXPECT_THAT(
        set.data(),
        ElementsAre((1ULL << 0) | (1ULL << 1) | (1ULL << 2) | (1ULL << 3) |
                    (1ULL << 4) | (1ULL << 20) | (1ULL << 23)));
  }
  {
    constexpr DenseSet<size_t, 0, 31> set{0, 1, 2, 3, 4, 20, 31};
    EXPECT_THAT(
        set.data(),
        ElementsAre((1ULL << 0) | (1ULL << 1) | (1ULL << 2) | (1ULL << 3) |
                    (1ULL << 4) | (1ULL << 20) | (1ULL << 31)));
  }
  {
    constexpr DenseSet<size_t, 0, 63> set{0, 1, 63};
    EXPECT_THAT(set.data(),
                ElementsAre((1ULL << 0) | (1ULL << 1) | (1ULL << 63)));
  }
}

TEST(DenseSetTest, iterators_begin_end) {
  enum class T : int {
    kMinusOne = -1,
    kOne = 1,
    kTwo = 2,
    kThree = 3,
    kFour = 4,
    kFive = 5,
    kMaxValue = kFive,
  };
  using DS = DenseSet<T, T::kMinusOne, T::kMaxValue>;

  DS s;
  s.insert(T::kTwo);
  s.insert(T::kFour);
  s.insert(T::kOne);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(std::distance(s.begin(), s.end()), 3);

  {
    auto it = s.begin();
    auto x1 = *it++;
    auto x2 = *it++;
    auto x3 = *it++;
    EXPECT_EQ(it, s.end());
    EXPECT_EQ(x1, T::kOne);
    EXPECT_EQ(x2, T::kTwo);
    EXPECT_EQ(x3, T::kFour);
  }

  {
    auto it = s.begin();
    auto x1 = *it;
    auto x2 = *++it;
    auto x3 = *++it;
    EXPECT_NE(it, s.end());
    EXPECT_EQ(x1, T::kOne);
    EXPECT_EQ(x2, T::kTwo);
    EXPECT_EQ(x3, T::kFour);
  }

  EXPECT_THAT(s, ::testing::ElementsAre(T::kOne, T::kTwo, T::kFour));
}

TEST(DenseSetTest, iterators_begin_end_reverse) {
  enum class T : int8_t {
    kMinusOne = -1,
    kOne = 1,
    kTwo = 2,
    kThree = 3,
    kFour = 4,
    kFive = 5,
    kMaxValue = kFive
  };
  using DS = DenseSet<T, T::kMinusOne, T::kMaxValue>;

  DS s;
  s.insert(T::kTwo);
  s.insert(T::kFour);
  s.insert(T::kOne);
  EXPECT_EQ(s.size(), 3u);

  {
    auto it = s.end();
    it--;
    auto x3 = *it--;
    auto x2 = *it--;
    auto x1 = *it;
    EXPECT_EQ(it, s.begin());
    EXPECT_EQ(x1, T::kOne);
    EXPECT_EQ(x2, T::kTwo);
    EXPECT_EQ(x3, T::kFour);
  }

  {
    auto it = s.end();
    auto x3 = *--it;
    auto x2 = *--it;
    auto x1 = *--it;
    EXPECT_EQ(it, s.begin());
    EXPECT_EQ(x1, T::kOne);
    EXPECT_EQ(x2, T::kTwo);
    EXPECT_EQ(x3, T::kFour);
  }
}

TEST(DenseSetTest, iterators_rbegin_rend) {
  enum class T {
    kMinusOne = -1,
    kOne = 1,
    kTwo = 2,
    kThree = 3,
    kFour = 4,
    kFive = 5,
    kMaxValue = kFive
  };
  using DS = DenseSet<T, T::kMinusOne, T::kMaxValue>;

  DS s;
  s.insert(T::kTwo);
  s.insert(T::kFour);
  s.insert(T::kOne);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(std::distance(s.rbegin(), s.rend()), 3);

  {
    auto it = s.rbegin();
    auto x3 = *it++;
    auto x2 = *it++;
    auto x1 = *it++;
    EXPECT_EQ(it, s.rend());
    EXPECT_EQ(x1, T::kOne);
    EXPECT_EQ(x2, T::kTwo);
    EXPECT_EQ(x3, T::kFour);
  }

  {
    auto it = s.rbegin();
    auto x3 = *it;
    auto x2 = *++it;
    auto x1 = *++it;
    EXPECT_NE(it, s.rend());
    EXPECT_EQ(x1, T::kOne);
    EXPECT_EQ(x2, T::kTwo);
    EXPECT_EQ(x3, T::kFour);
  }

  EXPECT_THAT(std::vector<T>(s.rbegin(), s.rend()),
              ::testing::ElementsAre(T::kFour, T::kTwo, T::kOne));
}

TEST(DenseSetTest, lookup) {
  enum class T {
    kMinusOne = -1,
    kOne = 1,
    kTwo = 2,
    kThree = 3,
    kFour = 4,
    kFive = 5,
    kMaxValue = kFive
  };
  using DS = DenseSet<T, T::kMinusOne, T::kMaxValue>;

  DS s;
  s.insert(T::kTwo);
  s.insert(T::kFour);
  s.insert(T::kOne);

  EXPECT_FALSE(s.contains(static_cast<T>(0)));
  EXPECT_TRUE(s.contains(T::kOne));
  EXPECT_TRUE(s.contains(T::kTwo));
  EXPECT_FALSE(s.contains(T::kThree));
  EXPECT_TRUE(s.contains(T::kFour));
  EXPECT_FALSE(s.contains(T::kFive));

  EXPECT_EQ(s.contains(static_cast<T>(0)), 0u);
  EXPECT_EQ(s.contains(T::kOne), 1u);
  EXPECT_EQ(s.contains(T::kTwo), 1u);
  EXPECT_EQ(s.contains(T::kThree), 0u);
  EXPECT_EQ(s.contains(T::kFour), 1u);
  EXPECT_EQ(s.contains(T::kFive), 0u);

  EXPECT_EQ(s.find(static_cast<T>(0)), s.end());
  EXPECT_NE(s.find(T::kOne), s.end());
  EXPECT_NE(s.find(T::kTwo), s.end());
  EXPECT_EQ(s.find(T::kThree), s.end());
  EXPECT_NE(s.find(T::kFour), s.end());
  EXPECT_EQ(s.find(T::kFive), s.end());

  EXPECT_EQ(*s.find(T::kOne), T::kOne);
  EXPECT_EQ(*s.find(T::kTwo), T::kTwo);
  EXPECT_EQ(*s.find(T::kFour), T::kFour);

  EXPECT_NE(s.find(static_cast<T>(0)), s.lower_bound(static_cast<T>(0)));
  EXPECT_EQ(s.find(T::kOne), s.lower_bound(T::kOne));
  EXPECT_EQ(s.find(T::kTwo), s.lower_bound(T::kTwo));
  EXPECT_NE(s.find(T::kThree), s.lower_bound(T::kThree));
  EXPECT_EQ(s.find(T::kFour), s.lower_bound(T::kFour));
  EXPECT_EQ(s.find(T::kFive), s.lower_bound(T::kFive));

  DS t;
  EXPECT_TRUE(t.empty());
  EXPECT_TRUE(t.contains_none({}));
  EXPECT_FALSE(t.contains_any({}));
  EXPECT_TRUE(t.contains_all({}));

  t.insert_all(s);
  EXPECT_EQ(s, t);
  EXPECT_FALSE(s.contains_none(t));
  EXPECT_TRUE(s.contains_any(t));
  EXPECT_TRUE(s.contains_all(t));
  EXPECT_TRUE(s.contains_none({}));
  EXPECT_FALSE(s.contains_any({}));
  EXPECT_TRUE(s.contains_all({}));

  t.erase(t.begin());
  EXPECT_FALSE(s.contains_none(t));
  EXPECT_TRUE(s.contains_any(t));
  EXPECT_TRUE(s.contains_all(t));
  EXPECT_FALSE(t.contains_none(s));
  EXPECT_FALSE(t.contains_all(s));
  EXPECT_TRUE(t.contains_any(s));
  EXPECT_TRUE(s.contains_none({}));
  EXPECT_FALSE(s.contains_any({}));
  EXPECT_TRUE(s.contains_all({}));
}

TEST(DenseSetTest, iterators_lower_upper_bound) {
  enum class T {
    kMinusOne = -1,
    kOne = 1,
    kTwo = 2,
    kThree = 3,
    kFour = 4,
    kFive = 5
  };
  using DS = DenseSet<T, T::kMinusOne, T::kFive>;

  DS s;
  s.insert(T::kTwo);
  s.insert(T::kFour);
  s.insert(T::kOne);
  EXPECT_EQ(s.size(), 3u);

  EXPECT_EQ(s.lower_bound(static_cast<T>(0)), s.begin());
  EXPECT_EQ(s.lower_bound(T::kOne), s.begin());

  EXPECT_EQ(s.upper_bound(T::kFour), s.end());
  EXPECT_EQ(s.upper_bound(T::kFive), s.end());

  {
    auto it = s.lower_bound(static_cast<T>(0));
    auto jt = s.upper_bound(static_cast<T>(0));
    EXPECT_EQ(it, jt);
  }

  {
    auto it = s.lower_bound(T::kOne);
    auto jt = s.upper_bound(T::kOne);
    auto x1 = *it++;
    EXPECT_EQ(it, jt);
    EXPECT_EQ(x1, T::kOne);
  }

  {
    auto it = s.lower_bound(T::kFour);
    auto jt = s.upper_bound(T::kFour);
    auto x3 = *it++;
    EXPECT_EQ(it, jt);
    EXPECT_EQ(x3, T::kFour);
  }

  {
    auto it = s.lower_bound(T::kFive);
    auto jt = s.upper_bound(T::kFive);
    EXPECT_EQ(it, jt);
  }

  {
    auto it = s.lower_bound(T::kOne);
    auto jt = s.upper_bound(T::kFive);
    auto x1 = *it++;
    auto x2 = *it++;
    auto x3 = *it++;
    EXPECT_EQ(it, jt);
    EXPECT_EQ(x1, T::kOne);
    EXPECT_EQ(x2, T::kTwo);
    EXPECT_EQ(x3, T::kFour);
  }

  {
    auto it = s.lower_bound(T::kThree);
    auto jt = s.upper_bound(T::kFour);
    auto x3 = *it++;
    EXPECT_EQ(jt, s.end());
    EXPECT_EQ(it, jt);
    EXPECT_EQ(x3, T::kFour);
  }

  EXPECT_EQ(static_cast<size_t>(std::distance(s.begin(), s.end())), s.size());
  EXPECT_EQ(std::next(std::next(std::next(s.begin()))), s.end());
}

TEST(DenseSetTest, max_size) {
  const int kOne = 1;
  const int kTwo = 2;
  // const int kThree = 3;
  const int kFour = 4;
  // const int kFive = 5;
  const int kMaxValue = 5;
  using DS = DenseSet<int, 0, kMaxValue>;

  DS s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
  EXPECT_EQ(s.max_size(), 6u);
  s.insert(kTwo);
  EXPECT_FALSE(s.empty());
  EXPECT_EQ(s.size(), 1u);
  s.insert(kFour);
  EXPECT_FALSE(s.empty());
  EXPECT_EQ(s.size(), 2u);
  s.insert(kOne);
  EXPECT_FALSE(s.empty());
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(s.max_size(), 6u);
}

TEST(DenseSetTest, modifiers) {
  const size_t kOne = 1;
  const size_t kTwo = 2;
  const size_t kThree = 3;
  const size_t kFour = 4;
  // const size_t kFive = 5;
  const size_t kMaxValue = 5;
  using DS = DenseSet<size_t, 0, kMaxValue>;

  DS s;
  s.insert(kTwo);
  s.insert(kFour);
  s.insert(kOne);
  EXPECT_EQ(s.size(), 3u);

  auto EXPECT_INSERTION = [](auto& set, auto value, bool took_place) {
    auto it = set.insert(value);
    EXPECT_EQ(it, std::make_pair(set.find(value), took_place));
  };

  DS t;
  EXPECT_NE(s, t);
  EXPECT_INSERTION(t, kTwo, true);
  EXPECT_INSERTION(t, kTwo, false);
  EXPECT_INSERTION(t, kFour, true);
  EXPECT_INSERTION(t, kFour, false);
  EXPECT_INSERTION(t, kOne, true);
  EXPECT_INSERTION(t, kOne, false);
  EXPECT_EQ(s, t);
  EXPECT_EQ(t.size(), 3u);

  EXPECT_INSERTION(t, kThree, true);
  EXPECT_INSERTION(t, kThree, false);
  EXPECT_EQ(t.erase(kThree), 1u);
  EXPECT_EQ(t.erase(kThree), 0u);
  EXPECT_EQ(s, t);
  EXPECT_EQ(t.size(), 3u);

  EXPECT_EQ(s.erase(kOne), 1u);
  EXPECT_EQ(t.erase(kFour), 1u);
  EXPECT_NE(s, t);
  EXPECT_EQ(s.size(), 2u);
  EXPECT_EQ(t.size(), 2u);

  EXPECT_INSERTION(s, kOne, true);
  EXPECT_INSERTION(t, kFour, true);
  EXPECT_EQ(s, t);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(t.size(), 3u);

  EXPECT_EQ(s.erase(s.find(kOne)), s.find(kTwo));
  EXPECT_EQ(t.erase(t.lower_bound(kOne), t.upper_bound(kOne)), t.find(kTwo));
  EXPECT_FALSE(s.contains(kOne));
  EXPECT_EQ(s, t);
  EXPECT_EQ(s.size(), 2u);
  EXPECT_EQ(t.size(), 2u);

  EXPECT_INSERTION(s, kOne, true);
  EXPECT_INSERTION(t, kOne, true);
  EXPECT_EQ(s, t);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(t.size(), 3u);

  EXPECT_EQ(s.erase(s.find(kTwo), s.end()), s.end());
  EXPECT_EQ(t.erase(t.lower_bound(kTwo), t.upper_bound(kFour)), t.end());
  EXPECT_TRUE(s.contains(kOne));
  EXPECT_EQ(s, t);
  EXPECT_EQ(s.size(), 1u);
  EXPECT_EQ(t.size(), 1u);

  EXPECT_INSERTION(s, kTwo, true);
  EXPECT_INSERTION(t, kTwo, true);
  EXPECT_INSERTION(s, kFour, true);
  EXPECT_INSERTION(t, kFour, true);
  EXPECT_EQ(s, t);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(t.size(), 3u);

  s.clear();
  EXPECT_EQ(s, DS());
  EXPECT_TRUE(s.empty());

  s.insert(kThree);
  s.insert_all(t);
  EXPECT_EQ(s.size(), 4u);
  EXPECT_EQ(t.size(), 3u);
  EXPECT_NE(s, t);
  EXPECT_FALSE(s.contains_none(t));
  EXPECT_TRUE(s.contains_any(t));
  EXPECT_TRUE(s.contains_all(t));
  EXPECT_TRUE(s.contains(kThree));
  EXPECT_FALSE(t.contains_none(s));
  EXPECT_TRUE(t.contains_any(s));
  EXPECT_FALSE(t.contains_all(s));

  s.erase_all(t);
  EXPECT_EQ(s.size(), 1u);
  EXPECT_TRUE(s.contains(kThree));
  EXPECT_TRUE(s.contains_none(t));
  EXPECT_FALSE(s.contains_any(t));
  EXPECT_FALSE(s.contains_all(t));

  s.insert_all(t);
  s.erase(kThree);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(s, t);

  s.erase_all(t);
  EXPECT_TRUE(s.empty());

  EXPECT_INSERTION(s, *t.begin(), true);
  EXPECT_TRUE(s.contains(kOne));
  EXPECT_INSERTION(s, *std::next(t.begin()), true);
  EXPECT_TRUE(s.contains(kTwo));
  EXPECT_INSERTION(s, *std::prev(t.end()), true);
  EXPECT_TRUE(s.contains(kFour));
  EXPECT_EQ(s, t);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(t.size(), 3u);
}

TEST(DenseSetTest, std_set) {
  constexpr size_t kMaxValue = 50;
  DenseSet<size_t, 0, kMaxValue> dense_set;
  std::set<size_t> std_set;

  auto expect_equivalence = [&] {
    EXPECT_EQ(dense_set.empty(), std_set.empty());
    EXPECT_EQ(dense_set.size(), std_set.size());
    EXPECT_TRUE(base::ranges::equal(dense_set, std_set));
  };

  auto random_insert = [&] {
    expect_equivalence();
    size_t value = base::RandUint64() % kMaxValue;
    auto p = dense_set.insert(value);
    auto q = std_set.insert(value);
    EXPECT_EQ(p.second, q.second);
    EXPECT_EQ(p.first == dense_set.end(), q.first == std_set.end());
    EXPECT_TRUE(!p.second || p.first == dense_set.find(value));
    EXPECT_TRUE(!q.second || q.first == std_set.find(value));
  };

  auto random_erase = [&] {
    expect_equivalence();
    size_t value = base::RandUint64() % kMaxValue;
    EXPECT_EQ(dense_set.erase(value), std_set.erase(value));
  };

  auto random_erase_iterator = [&] {
    expect_equivalence();
    size_t value = base::RandUint64() % kMaxValue;
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
    size_t min_value = base::RandUint64() % kMaxValue;
    size_t max_value = base::RandUint64() % kMaxValue;
    min_value = std::min(min_value, max_value);
    max_value = std::max(min_value, max_value);
    dense_set.erase(dense_set.lower_bound(min_value),
                    dense_set.upper_bound(max_value));
    std_set.erase(std_set.lower_bound(min_value),
                  std_set.upper_bound(max_value));
  };

  for (size_t i = 0; i < kMaxValue; ++i) {
    random_insert();
  }

  for (size_t i = 0; i < kMaxValue / 2; ++i) {
    random_erase();
  }

  expect_equivalence();
  dense_set.clear();
  std_set.clear();
  expect_equivalence();

  for (size_t i = 0; i < kMaxValue; ++i) {
    random_insert();
  }

  for (size_t i = 0; i < kMaxValue; ++i) {
    random_erase_iterator();
  }

  expect_equivalence();
  dense_set.clear();
  std_set.clear();
  expect_equivalence();

  for (size_t i = 0; i < kMaxValue; ++i) {
    random_insert();
  }

  for (size_t i = 0; i < kMaxValue; ++i) {
    random_erase_range();
  }

  expect_equivalence();
}

}  // namespace autofill
