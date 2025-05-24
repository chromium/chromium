// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_NODE_H_
#define COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_NODE_H_

// Base class representing a node in a DependencyGraph.
class DependencyNode {
 public:
  DependencyNode(const DependencyNode&) = delete;
  DependencyNode& operator=(const DependencyNode&) = delete;

 protected:
  // This is intended to be used by the subclasses, not directly.
  DependencyNode() = default;
  ~DependencyNode() = default;
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_NODE_H_
