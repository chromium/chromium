// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_WORK_ITEM_MOCKS_H_
#define CHROME_INSTALLER_UTIL_WORK_ITEM_MOCKS_H_

#include "chrome/installer/util/conditional_work_item_list.h"
#include "chrome/installer/util/work_item.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockWorkItem : public WorkItem {
 public:
  MockWorkItem();

  MockWorkItem(const MockWorkItem&) = delete;
  MockWorkItem& operator=(const MockWorkItem&) = delete;

  ~MockWorkItem() override;

  MOCK_METHOD0(DoImpl, bool());
  MOCK_METHOD0(RollbackImpl, void());
};

class MockCondition : public WorkItem::Condition {
 public:
  MockCondition();

  MockCondition(const MockCondition&) = delete;
  MockCondition& operator=(const MockCondition&) = delete;

  ~MockCondition() override;

  MOCK_CONST_METHOD0(ShouldRun, bool());
};

using StrictMockWorkItem = testing::StrictMock<MockWorkItem>;
using StrictMockCondition = testing::StrictMock<MockCondition>;

#endif  // CHROME_INSTALLER_UTIL_WORK_ITEM_MOCKS_H_
