// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_item_list.h"

#include <memory>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

namespace payments {

namespace {

class TestListItem : public PaymentRequestItemList::Item {
 public:
  TestListItem(PaymentRequestItemList* list, bool selected)
      : PaymentRequestItemList::Item(nullptr,
                                     nullptr,
                                     list,
                                     selected,
                                     /*clickable=*/true,
                                     /*show_edit_button=*/false),
        selected_state_changed_calls_count_(0) {
    Init();
  }

  int selected_state_changed_calls_count() {
    return selected_state_changed_calls_count_;
  }

 private:
  std::unique_ptr<views::View> CreateContentView(
      base::string16* accessible_content) override {
    return std::make_unique<views::View>();
  }

  base::string16 GetNameForDataType() override { return base::string16(); }

  bool CanBeSelected() override { return true; }

  void PerformSelectionFallback() override {}

  void EditButtonPressed() override {}

  void SelectedStateChanged() override {
    ++selected_state_changed_calls_count_;
  }

  int selected_state_changed_calls_count_;

  DISALLOW_COPY_AND_ASSIGN(TestListItem);
};

}  // namespace

TEST(PaymentRequestItemListTest, TestAddItem) {
  PaymentRequestItemList list(nullptr);

  std::unique_ptr<views::View> list_view = list.CreateListView();
  EXPECT_TRUE(list_view->children().empty());

  std::vector<std::unique_ptr<TestListItem>> items;
  items.push_back(std::make_unique<TestListItem>(&list, false));
  items.push_back(std::make_unique<TestListItem>(&list, true));
  items.push_back(std::make_unique<TestListItem>(&list, false));
  items.push_back(std::make_unique<TestListItem>(&list, true));

  // The unique_ptr objects will become owned by |list|, but the underlying
  // pointers will be needed for assertions after the unique_ptr is moved.
  std::vector<TestListItem*> item_pointers;
  for (auto& item : items) {
    item_pointers.push_back(item.get());
    list.AddItem(std::move(item));
  }

  EXPECT_FALSE(item_pointers[0]->selected());
  // Only one item should be selected at a time, so adding item at index 3 with
  // |selected| set to true should have deselected item at index 1.
  EXPECT_FALSE(item_pointers[1]->selected());
  EXPECT_FALSE(item_pointers[2]->selected());
  EXPECT_TRUE(item_pointers[3]->selected());

  list_view = list.CreateListView();
  EXPECT_EQ(4u, list_view->children().size());
}

TEST(PaymentRequestItemListTest, TestSelectItemResultsInSingleItemSelected) {
  PaymentRequestItemList list(nullptr);

  std::vector<std::unique_ptr<TestListItem>> items;
  items.push_back(std::make_unique<TestListItem>(&list, false));
  items.push_back(std::make_unique<TestListItem>(&list, false));
  items.push_back(std::make_unique<TestListItem>(&list, false));

  // The unique_ptr objects will become owned by |list|, but the underlying
  // pointers will be needed for assertions after the unique_ptr is moved.
  std::vector<TestListItem*> item_pointers;
  for (auto& item : items) {
    item_pointers.push_back(item.get());
    list.AddItem(std::move(item));
  }

  // Only one item should be selected at once and items should have their
  // SelectedStateChanged() function called when they are selected and when
  // they are unselected.
  list.SelectItem(item_pointers[0]);
  EXPECT_TRUE(item_pointers[0]->selected());
  EXPECT_EQ(1, item_pointers[0]->selected_state_changed_calls_count());

  EXPECT_FALSE(item_pointers[1]->selected());
  EXPECT_EQ(0, item_pointers[1]->selected_state_changed_calls_count());

  EXPECT_FALSE(item_pointers[2]->selected());
  EXPECT_EQ(0, item_pointers[2]->selected_state_changed_calls_count());

  list.SelectItem(item_pointers[2]);
  EXPECT_FALSE(item_pointers[0]->selected());
  EXPECT_EQ(2, item_pointers[0]->selected_state_changed_calls_count());

  EXPECT_FALSE(item_pointers[1]->selected());
  EXPECT_EQ(0, item_pointers[1]->selected_state_changed_calls_count());

  EXPECT_TRUE(item_pointers[2]->selected());
  EXPECT_EQ(1, item_pointers[2]->selected_state_changed_calls_count());

  list.SelectItem(item_pointers[1]);
  EXPECT_FALSE(item_pointers[0]->selected());
  EXPECT_EQ(2, item_pointers[0]->selected_state_changed_calls_count());

  EXPECT_TRUE(item_pointers[1]->selected());
  EXPECT_EQ(1, item_pointers[1]->selected_state_changed_calls_count());

  EXPECT_FALSE(item_pointers[2]->selected());
  EXPECT_EQ(2, item_pointers[2]->selected_state_changed_calls_count());
}

}  // namespace payments
