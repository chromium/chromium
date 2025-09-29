// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/mock_tab_group.h"

namespace tabs {

MockTabGroup::MockTabGroup(tabs::TabGroupTabCollection* collection,
                           const tab_groups::TabGroupId& id,
                           const tab_groups::TabGroupVisualData& visual_data)
    : testing::NiceMock<TabGroup>(collection, id, visual_data) {}

MockTabGroup::~MockTabGroup() = default;

MockTabGroupFactory::MockTabGroupFactory(Profile* profile)
    : testing::NiceMock<TabGroup::Factory>(profile) {}

MockTabGroupFactory::~MockTabGroupFactory() = default;

}  // namespace tabs
