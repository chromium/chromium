// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_GRAPH_IMPL_UTIL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_GRAPH_IMPL_UTIL_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"

namespace performance_manager {

template <typename PublicNodeClass, typename NodeImplClass>
base::flat_set<const PublicNodeClass*> UpcastNodeSet(
    const base::flat_set<raw_ptr<NodeImplClass, CtnExperimental>>& node_set) {
  // As node_set is a flat_set, its contents are sorted and unique already.
  return base::flat_set<const PublicNodeClass*>(
      base::sorted_unique, node_set.begin(), node_set.end());
}

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_GRAPH_IMPL_UTIL_H_
