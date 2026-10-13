// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"

#include <compare>
#include <memory>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

// Has no comparison operators, only a Compare() method.
struct ComparableItem {
  int id;
  std::string value;

  int Compare(const ComparableItem& other) const {
    if (id != other.id) {
      return id < other.id ? -1 : 1;
    }
    return value.compare(other.value);
  }
};

struct LessComparableItem {
  int id;
  std::string value;

  friend bool operator==(const LessComparableItem&,
                         const LessComparableItem&) = default;
  friend auto operator<=>(const LessComparableItem&,
                          const LessComparableItem&) = default;
};

// Has no comparison operators, only a SerializeAsString() method, like protos.
struct SerializableItem {
  std::string data;

  std::string SerializeAsString() const { return data; }
};

bool CompareLess(const ComparableItem& a, const ComparableItem& b) {
  return a.Compare(b) < 0;
}

const ComparableItem& Deref(const std::unique_ptr<ComparableItem>& item) {
  return *item;
}

std::string Serialize(const SerializableItem& item) {
  return item.SerializeAsString();
}

TEST(AutofillSyncBridgeUtilTest, AreAnyItemsDifferent_ComparisonOperators) {
  // Test empty vectors.
  std::vector<LessComparableItem> old_items;
  std::vector<LessComparableItem> new_items;
  EXPECT_FALSE(AreAnyItemsDifferent(old_items, new_items));

  // Test different sizes.
  new_items.push_back({1, "a"});
  EXPECT_TRUE(AreAnyItemsDifferent(old_items, new_items));

  // Test same elements, same order.
  old_items.push_back({1, "a"});
  EXPECT_FALSE(AreAnyItemsDifferent(old_items, new_items));

  // Test same elements, different order.
  old_items.push_back({2, "b"});
  new_items.insert(new_items.begin(), {2, "b"});
  EXPECT_FALSE(AreAnyItemsDifferent(old_items, new_items));

  // Test different elements.
  new_items[0].value = "c";
  EXPECT_TRUE(AreAnyItemsDifferent(old_items, new_items));
}

// Tests comparing a vector of unique_ptrs with a vector of values, using a
// custom comparator.
TEST(AutofillSyncBridgeUtilTest,
     AreAnyItemsDifferent_UniquePtrWithComparatorAndProjection) {
  // Test empty vectors.
  std::vector<std::unique_ptr<ComparableItem>> old_items;
  std::vector<ComparableItem> new_items;
  EXPECT_FALSE(AreAnyItemsDifferent(old_items, new_items, CompareLess, Deref));

  // Test different sizes.
  new_items.push_back({1, "a"});
  EXPECT_TRUE(AreAnyItemsDifferent(old_items, new_items, CompareLess, Deref));

  // Test same elements, same order.
  old_items.push_back(std::make_unique<ComparableItem>(ComparableItem{1, "a"}));
  EXPECT_FALSE(AreAnyItemsDifferent(old_items, new_items, CompareLess, Deref));

  // Test same elements, different order.
  old_items.push_back(std::make_unique<ComparableItem>(ComparableItem{2, "b"}));
  new_items.insert(new_items.begin(), {2, "b"});
  EXPECT_FALSE(AreAnyItemsDifferent(old_items, new_items, CompareLess, Deref));

  // Test different elements.
  new_items[0].value = "c";
  EXPECT_TRUE(AreAnyItemsDifferent(old_items, new_items, CompareLess, Deref));
}

// Tests comparing items by a projection, e.g. the serialized string of a proto.
TEST(AutofillSyncBridgeUtilTest, AreAnyItemsDifferent_SerializedComparison) {
  // Test empty vectors.
  std::vector<SerializableItem> old_items;
  std::vector<SerializableItem> new_items;
  EXPECT_FALSE(
      AreAnyItemsDifferent(old_items, new_items, {}, Serialize, Serialize));

  // Test different sizes.
  new_items.push_back({"a"});
  EXPECT_TRUE(
      AreAnyItemsDifferent(old_items, new_items, {}, Serialize, Serialize));

  // Test same elements, same order.
  old_items.push_back({"a"});
  EXPECT_FALSE(
      AreAnyItemsDifferent(old_items, new_items, {}, Serialize, Serialize));

  // Test same elements, different order.
  old_items.push_back({"b"});
  new_items.insert(new_items.begin(), {"b"});
  EXPECT_FALSE(
      AreAnyItemsDifferent(old_items, new_items, {}, Serialize, Serialize));

  // Test different elements.
  new_items[0].data = "c";
  EXPECT_TRUE(
      AreAnyItemsDifferent(old_items, new_items, {}, Serialize, Serialize));
}

// Tests that duplicates are taken into account, i.e. the vectors are compared
// as multisets rather than sets.
TEST(AutofillSyncBridgeUtilTest, AreAnyItemsDifferent_Duplicates) {
  std::vector<LessComparableItem> old_items = {{1, "a"}, {1, "a"}, {2, "b"}};
  std::vector<LessComparableItem> new_items = {{1, "a"}, {2, "b"}, {2, "b"}};
  EXPECT_TRUE(AreAnyItemsDifferent(old_items, new_items));

  new_items = {{2, "b"}, {1, "a"}, {1, "a"}};
  EXPECT_FALSE(AreAnyItemsDifferent(old_items, new_items));
}

}  // namespace
}  // namespace autofill
