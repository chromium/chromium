// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_MOCK_TAB_GROUP_H_
#define COMPONENTS_TABS_PUBLIC_MOCK_TAB_GROUP_H_

#include <memory>
#include <string>

#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;
class TabGroupFeatures;

namespace tabs {
class TabGroupTabCollection;

class MockTabGroup : public testing::NiceMock<TabGroup> {
 public:
  MockTabGroup(tabs::TabGroupTabCollection* collection,
               const tab_groups::TabGroupId& id,
               const tab_groups::TabGroupVisualData& visual_data);

  ~MockTabGroup() override;

  MOCK_METHOD(TabGroupFeatures*, GetTabGroupFeatures, (), (override));
  MOCK_METHOD(const TabGroupFeatures*,
              GetTabGroupFeatures,
              (),
              (const override));
};

class MockTabGroupFactory : public testing::NiceMock<TabGroup::Factory> {
 public:
  explicit MockTabGroupFactory(Profile* profile);

  ~MockTabGroupFactory() override;

  MOCK_METHOD(std::unique_ptr<TabGroup>,
              Create,
              (tabs::TabGroupTabCollection*,
               const tab_groups::TabGroupId&,
               const tab_groups::TabGroupVisualData&),
              (override));
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_MOCK_TAB_GROUP_H_
