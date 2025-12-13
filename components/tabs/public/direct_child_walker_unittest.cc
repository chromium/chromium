// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/direct_child_walker.h"

#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_collection.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {

namespace {

class MockProcessor : public DirectChildWalker::Processor {
 public:
  MOCK_METHOD(void, ProcessTab, (const TabInterface* tab), (override));
  MOCK_METHOD(void,
              ProcessCollection,
              (const TabCollection* collection),
              (override));
};

class MockTabCollection : public TabCollection {
 public:
  MockTabCollection()
      : TabCollection(TabCollection::Type::TABSTRIP,
                      {TabCollection::Type::GROUP},
                      true) {}

  MOCK_METHOD(const ChildrenVector&,
              GetChildren,
              (base::PassKey<DirectChildWalker>),
              (const, override));
};

class DirectChildWalkerTest : public testing::Test {
 protected:
  MockProcessor mock_processor_;
  testing::StrictMock<MockTabCollection> collection_;
};

TEST_F(DirectChildWalkerTest, WalksEmptyCollection) {
  ChildrenVector children;
  EXPECT_CALL(collection_, GetChildren(testing::_))
      .WillOnce(testing::ReturnRef(children));

  DirectChildWalker walker(&collection_, &mock_processor_);
  walker.Walk();
}

TEST_F(DirectChildWalkerTest, WalksMixedCollectionInOrder) {
  ChildrenVector children;
  children.emplace_back(std::make_unique<MockTabInterface>());
  auto* tab1_ptr =
      std::get<std::unique_ptr<TabInterface>>(children.back()).get();
  children.emplace_back(std::make_unique<MockTabCollection>());
  auto* collection_ptr =
      std::get<std::unique_ptr<TabCollection>>(children.back()).get();
  children.emplace_back(std::make_unique<MockTabInterface>());
  auto* tab2_ptr =
      std::get<std::unique_ptr<TabInterface>>(children.back()).get();

  EXPECT_CALL(collection_, GetChildren(testing::_))
      .WillOnce(testing::ReturnRef(children));

  {
    testing::InSequence s;
    EXPECT_CALL(mock_processor_, ProcessTab(tab1_ptr));
    EXPECT_CALL(mock_processor_, ProcessCollection(collection_ptr));
    EXPECT_CALL(mock_processor_, ProcessTab(tab2_ptr));
  }

  DirectChildWalker walker(&collection_, &mock_processor_);
  walker.Walk();
}

}  // namespace

}  // namespace tabs
