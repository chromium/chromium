// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_H_

#include <cstdint>

#include "base/macros.h"

namespace performance_manager {

class Graph;

// Interface that all nodes must implement.
// TODO(chrisha): Move NodeTypeEnum to the public interface and expose it here,
// then add FromNode casts on the public node interfaces.
class Node {
 public:
  Node();
  virtual ~Node();

  // Returns the graph to which this node belongs.
  virtual Graph* GetGraph() const = 0;

  // The following functions are implementation detail and should not need to be
  // used by external clients. They provide the ability to safely downcast to
  // the underlying implementation.
  virtual uintptr_t GetImplType() const = 0;
  virtual const void* GetImpl() const = 0;

  // Returns the serialization ID of the given |node|. This is a stable and
  // opaque value that will always refer only to this node, and never be reused
  // over the lifetime of the browser.
  // TODO(chrisha): Deprecate this, and move the logic inside of the only
  // client using it.
  static int64_t GetSerializationId(const Node* node);

 private:
  DISALLOW_COPY_AND_ASSIGN(Node);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_NODE_H_
