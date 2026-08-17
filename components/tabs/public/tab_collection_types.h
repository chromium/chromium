// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_TYPES_H_
#define COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_TYPES_H_

#include <memory>
#include <variant>
#include <vector>

#include "base/memory/raw_ptr.h"

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

// TODO(crbug.com/546526636): Resolve the dangling pointers here by ensuring
// ownership is handled correctly and removing DanglingUntriaged.
using DanglingUntriagedTabInterface =
    raw_ptr<tabs::TabInterface, DanglingUntriaged>;
using DanglingUntriagedTabCollection =
    raw_ptr<tabs::TabCollection, DanglingUntriaged>;
using ConstDanglingUntriagedTabInterface =
    raw_ptr<const tabs::TabInterface, DanglingUntriaged>;
using ConstDanglingUntriagedTabCollection =
    raw_ptr<const tabs::TabCollection, DanglingUntriaged>;

using ChildPtr =
    std::variant<DanglingUntriagedTabInterface, DanglingUntriagedTabCollection>;
using ChildrenPtrs = std::vector<ChildPtr>;
using ConstChildPtr = std::variant<ConstDanglingUntriagedTabInterface,
                                   ConstDanglingUntriagedTabCollection>;
using ConstChildrenPtrs = std::vector<ConstChildPtr>;

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_TYPES_H_
