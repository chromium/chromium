// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_TYPES_H_
#define COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_TYPES_H_

#include <memory>
#include <variant>
#include <vector>

namespace tabs {

class TabInterface;
class TabCollection;

// Custom deleter for TabInterface in tab collections that delegates destruction
// to TabInterface::DeleteSelf().
struct TabDeleter {
  constexpr TabDeleter() = default;
  void operator()(TabInterface* tab) const;
};

using ScopedTab = std::unique_ptr<TabInterface, TabDeleter>;

using Child = std::variant<std::unique_ptr<TabCollection>, ScopedTab>;
using ChildrenVector = std::vector<Child>;

using ChildPtr = std::variant<tabs::TabInterface*, tabs::TabCollection*>;
using ChildrenPtrs = std::vector<ChildPtr>;
using ConstChildPtr =
    std::variant<const tabs::TabInterface*, const tabs::TabCollection*>;
using ConstChildrenPtrs = std::vector<ConstChildPtr>;

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_TYPES_H_
