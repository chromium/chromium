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

using Child =
    std::variant<std::unique_ptr<TabCollection>, std::unique_ptr<TabInterface>>;
using ChildrenVector = std::vector<Child>;

using ChildPtr = std::variant<tabs::TabInterface*, tabs::TabCollection*>;
using ChildrenPtrs = std::vector<ChildPtr>;
using ConstChildPtr =
    std::variant<const tabs::TabInterface*, const tabs::TabCollection*>;
using ConstChildrenPtrs = std::vector<ConstChildPtr>;

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_TYPES_H_
