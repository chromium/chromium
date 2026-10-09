// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_FAKE_TAB_INTERFACE_H_
#define COMPONENTS_TABS_PUBLIC_FAKE_TAB_INTERFACE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/mock_tab_interface.h"

namespace tabs {

class TabCollection;

// A MockTabInterface that behaves like a real tab within a TabCollection
// hierarchy. It records the collection it is parented to and, mirroring
// TabModel, derives its pinned, group and split state from its ancestors
// whenever it or one of its ancestors is reparented.
//
// Use this instead of a plain MockTabInterface when exercising TabCollection
// logic that relies on that state, e.g. TabCollection::TabIterator or
// TabStripCollection::CreateSplit(). As with MockTabInterface, any of these
// defaults can be overridden with ON_CALL() or EXPECT_CALL().
class FakeTabInterface : public MockTabInterface {
 public:
  FakeTabInterface();
  FakeTabInterface(const FakeTabInterface&) = delete;
  FakeTabInterface& operator=(const FakeTabInterface&) = delete;
  ~FakeTabInterface() override;

 private:
  // Recomputes the pinned, group and split state from `parent_collection_`
  // and its ancestors.
  void UpdateProperties();

  raw_ptr<TabCollection> parent_collection_ = nullptr;
  bool pinned_ = false;
  std::optional<tab_groups::TabGroupId> group_;
  std::optional<split_tabs::SplitTabId> split_;
};

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_FAKE_TAB_INTERFACE_H_
