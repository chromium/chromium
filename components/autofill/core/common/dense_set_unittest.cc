// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/dense_set.h"

#include <vector>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace autofill {

template <auto kMinValueP, auto kMaxValueP>
struct DenseSetTraitsWrapper {
  static constexpr auto kMinValue = kMinValueP;
  static constexpr auto kMaxValue = kMaxValueP;
  static constexpr bool kPacked = false;
};

template <typename T, T kMinValue = T::kMinValue, T kMaxValue = T::kMaxValue>
using DenseSetWrapper =
    DenseSet<T, DenseSetTraitsWrapper<kMinValue, kMaxValue>>;

TEST(DenseSetTest, size_of) {
  static_assert(sizeof(DenseSetWrapper<size_t, 0, 1>) == 1u);
  static_assert(sizeof(DenseSetWrapper<size_t, 0, 7>) == 1u);
  static_assert(sizeof(DenseSetWrapper<size_t, 0, 8>) == 2u);
  static_assert(sizeof(DenseSetWrapper<size_t, 0, 15>) == 2u);
  static_assert(sizeof(DenseSetWrapper<size_t, 0, 16>) == 4u);
  static_assert(sizeof(DenseSetWrapper<size_t, 0, 31>) == 4u);
  static_assert(sizeof(DenseSetWrapper<size_t, 0, 32>) == 8u);
  static_assert(sizeof(DenseSetWrapper<size_t, 0, 63>) == 8u);
  static_assert(sizeof(DenseSetWrapper<size_t, 0, 64>) == 16u);
  static_assert(sizeof(DenseSetWrapper<size_t, 0, 127>) == 16u);
  static_assert(sizeof(DenseSetWrapper<size_t, 0, 255>) == 32u);
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
  DenseSet<T> s;
  EXPECT_TRUE(s.empty());
  EXPECT_EQ(s.size(), 0u);
  EXPECT_EQ(DenseSet<T>(s.begin(), s.end()), s);
  s.insert(T::kTwo);
  s.insert(T::kFour);
  s.insert(T::kOne);
  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(DenseSet<T>(s.begin(), s.end()), s);
  EXPECT_EQ(DenseSet<T>(s.cbegin(), s.cend()), s);
  EXPECT_EQ(DenseSet<T>(s.rbegin(), s.rend()), s);
  EXPECT_EQ(DenseSet<T>(s.crbegin(), s.crend()), s);
  EXPECT_EQ(DenseSet<T>({T::kFour, T::kTwo, T::kOne}), s);
}

TEST(DenseSetTest, FromRange) {
  enum class E { kOne = 1, kTwo = 2, kThree = 3, kMaxValue = kThree };
  struct S {
    auto operator<=>(const S&) const = default;

    E e = E::kOne;
  };

  {
    std::vector<S> container = {S{.e = E::kTwo}, S{.e = E::kThree}};
    DenseSet s(container, &S::e);
    EXPECT_EQ(s, DenseSet({E::kTwo, E::kThree}));
  }

  {
    std::set<S> container = {S{.e = E::kOne}, S{.e = E::kThree}};
    DenseSet s(container, &S::e);
    EXPECT_EQ(s, DenseSet({E::kThree, E::kOne}));
  }
}

TEST(DenseSetTest, initializer_list) {
  // The largest value so that DenseSet offers a constexpr constructor.
  constexpr uint64_t kMaxValueForConstexpr = 63;

  // Each of the below blocks is a copy that only varies in `kMax` and whether
  // or not the `set` is `constexpr`.

  {
    constexpr uint64_t kMax = 10;
    constexpr DenseSetWrapper<uint64_t, 0, kMax> set{0, 1, kMax - 2, kMax - 1,
                                                     kMax};
    EXPECT_THAT(std::vector<uint64_t>(set.begin(), set.end()),
                ElementsAre(0, 1, kMax - 2, kMax - 1, kMax));
  }

  {
    constexpr uint64_t kMax = kMaxValueForConstexpr;
    constexpr DenseSetWrapper<uint64_t, 0, kMax> set{0, 1, kMax - 2, kMax - 1,
                                                     kMax};
    EXPECT_THAT(std::vector<uint64_t>(set.begin(), set.end()),
                ElementsAre(0, 1, kMax - 2, kMax - 1, kMax));
  }

  {
    constexpr uint64_t kMax = kMaxValueForConstexpr + 1;
    DenseSetWrapper<uint64_t, 0, kMax> set{0, 1, kMax - 2, kMax - 1, kMax};
    EXPECT_THAT(std::vector<uint64_t>(set.begin(), set.end()),
                ElementsAre(0, 1, kMax - 2, kMax - 1, kMax));
  }

  {
    constexpr uint64_t kMax = kMaxValueForConstexpr + 2;
    DenseSetWrapper<uint64_t, 0, kMax> set{0, 1, kMax - 2, kMax - 1, kMax};
    EXPECT_THAT(std::vector<uint64_t>(set.begin(), set.end()),
                ElementsAre(0, 1, kMax - 2, kMax - 1, kMax));
  }

  {
    constexpr uint64_t kMax = kMaxValueForConstexpr + 100;
    DenseSetWrapper<uint64_t, 0, kMax> set{0, 1, kMax - 2, kMax - 1, kMax};
    EXPECT_THAT(std::vector<uint64_t>(set.begin(), set.end()),
                ElementsAre(0, 1, kMax - 2, kMax - 1, kMax));
  }
}

TEST(DenseSetTest, all_non_enum) {
  constexpr DenseSetWrapper<int, 0, 10> set =
      DenseSetWrapper<int, 0, 10>::all();
  EXPECT_EQ(set.size(), 11u);
  EXPECT_THAT(std::vector<int>(set.begin(), set.end()),
              ElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
}

TEST(DenseSetTest, all_enum) {
  enum class T {
    kMinusOne = -1,
    kOne = 1,
    kTwo = 2,
    kMaxValue = kTwo,
  };

  constexpr DenseSetWrapper<T, T::kMinusOne> set =
      DenseSetWrapper<T, T::kMinusOne>::all();
  // `set` will contain all values from -1 to 2, including 0 even if 0 doesn't
  // correspond to any value of `T`.
  EXPECT_EQ(set.size(), 4u);
  EXPECT_THAT(std::vector<T>(set.begin(), set.end()),
              ElementsAre(T::kMinusOne, static_cast<T>(0), T::kOne, T::kTwo));
}

TEST(DenseSetTest, data) {
  {
    constexpr DenseSetWrapper<uint64_t, 0, 23> set{0, 1, 2, 3, 4, 20, 23};
    EXPECT_THAT(
        set.data(),
        ElementsAre((1ULL << 0) | (1ULL << 1) | (1ULL << 2) | (1ULL << 3) |
                    (1ULL << 4) | (1ULL << 20) | (1ULL << 23)));
  }
  {
    constexpr DenseSetWrapper<uint64_t, 0, 31> set{0, 1, 2, 3, 4, 20, 31};
    EXPECT_THAT(
        set.data(),
        ElementsAre((1ULL << 0) | (1ULL << 1) | (1ULL << 2) | (1ULL << 3) |
                    (1ULL << 4) | (1ULL << 20) | (1ULL << 31)));
  }
  {
    constexpr DenseSetWrapper<uint64_t, 0, 63> set{0, 1, 63};
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
  DenseSetWrapper<T, T::kMinusOne> s;
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

  EXPECT_THAT(s, ElementsAre(T::kOne, T::kTwo, T::kFour));
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

  DenseSetWrapper<T, T::kMinusOne> s;
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

  DenseSetWrapper<T, T::kMinusOne> s;
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
              ElementsAre(T::kFour, T::kTwo, T::kOne));
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

  DenseSetWrapper<T, T::kMinusOne> s;
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

  DenseSetWrapper<T, T::kMinusOne> t;
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

  DenseSetWrapper<T, T::kMinusOne, T::kFive> s;
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

  DenseSetWrapper<int, 0, kMaxValue> s;
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
  const uint64_t kOne = 1;
  const uint64_t kTwo = 2;
  const uint64_t kThree = 3;
  const uint64_t kFour = 4;
  // const uint64_t kFive = 5;
  const uint64_t kMaxValue = 5;

  DenseSetWrapper<uint64_t, 0, kMaxValue> s;
  s.insert(kTwo);
  s.insert(kFour);
  s.insert(kOne);
  EXPECT_EQ(s.size(), 3u);

  auto EXPECT_INSERTION = [](auto& set, auto value, bool took_place) {
    auto it = set.insert(value);
    EXPECT_EQ(it, std::make_pair(set.find(value), took_place));
  };

  DenseSetWrapper<uint64_t, 0, kMaxValue> t;
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
  EXPECT_EQ(s, decltype(s){});
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

TEST(DenseSetTest, intersect) {
  constexpr uint64_t kMaxValue = 5;
  DenseSetWrapper<uint64_t, 0, kMaxValue> s = {1, 3, 4};
  DenseSetWrapper<uint64_t, 0, kMaxValue> t = {1, 2, 4};
  s.intersect(t);
  // Expect that only 1 and 4 remain.
  t.erase(2);
  EXPECT_EQ(s, t);
}

TEST(DenseSetTest, std_set) {
  constexpr uint64_t kMaxValue = 50;
  DenseSetWrapper<uint64_t, 0, kMaxValue> dense_set;
  std::set<uint64_t> std_set;

  auto expect_equivalence = [&] {
    EXPECT_EQ(dense_set.empty(), std_set.empty());
    EXPECT_EQ(dense_set.size(), std_set.size());
    EXPECT_TRUE(base::ranges::equal(dense_set, std_set));
  };

  auto random_insert = [&] {
    expect_equivalence();
    uint64_t value = base::RandUint64() % kMaxValue;
    auto p = dense_set.insert(value);
    auto q = std_set.insert(value);
    EXPECT_EQ(p.second, q.second);
    EXPECT_EQ(p.first == dense_set.end(), q.first == std_set.end());
    EXPECT_TRUE(!p.second || p.first == dense_set.find(value));
    EXPECT_TRUE(!q.second || q.first == std_set.find(value));
  };

  auto random_erase = [&] {
    expect_equivalence();
    uint64_t value = base::RandUint64() % kMaxValue;
    EXPECT_EQ(dense_set.erase(value), std_set.erase(value));
  };

  auto random_erase_iterator = [&] {
    expect_equivalence();
    uint64_t value = base::RandUint64() % kMaxValue;
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
    uint64_t min_value = base::RandUint64() % kMaxValue;
    uint64_t max_value = base::RandUint64() % kMaxValue;
    min_value = std::min(min_value, max_value);
    max_value = std::max(min_value, max_value);
    dense_set.erase(dense_set.lower_bound(min_value),
                    dense_set.upper_bound(max_value));
    std_set.erase(std_set.lower_bound(min_value),
                  std_set.upper_bound(max_value));
  };

  for (uint64_t i = 0; i < kMaxValue; ++i) {
    random_insert();
  }

  for (uint64_t i = 0; i < kMaxValue / 2; ++i) {
    random_erase();
  }

  expect_equivalence();
  dense_set.clear();
  std_set.clear();
  expect_equivalence();

  for (uint64_t i = 0; i < kMaxValue; ++i) {
    random_insert();
  }

  for (uint64_t i = 0; i < kMaxValue; ++i) {
    random_erase_iterator();
  }

  expect_equivalence();
  dense_set.clear();
  std_set.clear();
  expect_equivalence();

  for (uint64_t i = 0; i < kMaxValue; ++i) {
    random_insert();
  }

  for (uint64_t i = 0; i < kMaxValue; ++i) {
    random_erase_range();
  }

  expect_equivalence();
}

}  // namespace autofill
