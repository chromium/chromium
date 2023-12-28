// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_item_list.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_request_spec.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "ui/views/view.h"

namespace payments {
namespace {

std::unique_ptr<PaymentRequestSpec> BuildSpec() {
  return std::make_unique<PaymentRequestSpec>(
      mojom::PaymentOptions::New(), mojom::PaymentDetails::New(),
      std::vector<mojom::PaymentMethodDataPtr>(),
      /*observer=*/nullptr, /*app_locale=*/"en-US");
}

class TestListItem : public PaymentRequestItemList::Item {
 public:
  TestListItem(base::WeakPtr<PaymentRequestSpec> spec,
               PaymentRequestItemList* list,
               bool selected)
      : PaymentRequestItemList::Item(spec,
                                     /*state=*/nullptr,
                                     list,
                                     selected,
                                     /*clickable=*/true,
                                     /*show_edit_button=*/false) {
    Init();
  }

  TestListItem(const TestListItem&) = delete;
  TestListItem& operator=(const TestListItem&) = delete;

  int selected_state_changed_calls_count() {
    return selected_state_changed_calls_count_;
  }

  base::WeakPtr<PaymentRequestRowView> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<views::View> CreateContentView(
      std::u16string* accessible_content) override {
    return std::make_unique<views::View>();
  }

  std::u16string GetNameForDataType() override { return std::u16string(); }

  bool CanBeSelected() override { return true; }

  void PerformSelectionFallback() override {}

  void EditButtonPressed() override {}

  void SelectedStateChanged() override {
    ++selected_state_changed_calls_count_;
  }

  int selected_state_changed_calls_count_ = 0;
  base::WeakPtrFactory<TestListItem> weak_ptr_factory_{this};
};

}  // namespace

TEST(PaymentRequestItemListTest, TestAddItem) {
  PaymentRequestItemList list(nullptr);

  std::unique_ptr<views::View> list_view = list.CreateListView();
  EXPECT_TRUE(list_view->children().empty());

  std::unique_ptr<PaymentRequestSpec> spec = BuildSpec();
  std::vector<std::unique_ptr<TestListItem>> items;
  items.push_back(
      std::make_unique<TestListItem>(spec->AsWeakPtr(), &list, false));
  items.push_back(
      std::make_unique<TestListItem>(spec->AsWeakPtr(), &list, true));
  items.push_back(
      std::make_unique<TestListItem>(spec->AsWeakPtr(), &list, false));
  items.push_back(
      std::make_unique<TestListItem>(spec->AsWeakPtr(), &list, true));

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

  std::unique_ptr<PaymentRequestSpec> spec = BuildSpec();
  std::vector<std::unique_ptr<TestListItem>> items;
  items.push_back(
      std::make_unique<TestListItem>(spec->AsWeakPtr(), &list, false));
  items.push_back(
      std::make_unique<TestListItem>(spec->AsWeakPtr(), &list, false));
  items.push_back(
      std::make_unique<TestListItem>(spec->AsWeakPtr(), &list, false));

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
