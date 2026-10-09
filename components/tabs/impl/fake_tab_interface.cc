// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/fake_tab_interface.h"

#include "base/types/pass_key.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace tabs {

FakeTabInterface::FakeTabInterface() {
  ON_CALL(*this, OnReparented)
      .WillByDefault(
          [this](TabCollection* parent, base::PassKey<TabCollection>) {
            parent_collection_ = parent;
            UpdateProperties();
          });
  ON_CALL(*this, OnAncestorChanged)
      .WillByDefault(
          [this](base::PassKey<TabCollection>) { UpdateProperties(); });
  ON_CALL(*this,
          GetParentCollection(testing::An<base::PassKey<TabCollection>>()))
      .WillByDefault([this]() -> TabCollection* { return parent_collection_; });
  ON_CALL(*this, GetParentCollection())
      .WillByDefault(
          [this]() -> const TabCollection* { return parent_collection_; });
  ON_CALL(*this, IsPinned).WillByDefault([this] { return pinned_; });
  ON_CALL(*this, IsSplit).WillByDefault([this] { return split_.has_value(); });
  ON_CALL(*this, GetGroup).WillByDefault([this] { return group_; });
  ON_CALL(*this, GetSplit).WillByDefault([this] { return split_; });
}

FakeTabInterface::~FakeTabInterface() = default;

void FakeTabInterface::UpdateProperties() {
  // Unlike TabModel, state is also cleared when the tab is detached, so tests
  // do not need to flag tabs as detaching before removing them.
  pinned_ = false;
  group_.reset();
  split_.reset();

  for (const TabCollection* ancestor = parent_collection_; ancestor;
       ancestor = ancestor->GetParentCollection()) {
    switch (ancestor->type()) {
      case TabCollection::Type::PINNED:
        pinned_ = true;
        break;
      case TabCollection::Type::GROUP:
        group_ = static_cast<const TabGroupTabCollection*>(ancestor)
                     ->GetTabGroupId();
        break;
      case TabCollection::Type::SPLIT:
        split_ =
            static_cast<const SplitTabCollection*>(ancestor)->GetSplitTabId();
        break;
      case TabCollection::Type::TABSTRIP:
      case TabCollection::Type::UNPINNED:
        break;
    }
  }
}

}  // namespace tabs
