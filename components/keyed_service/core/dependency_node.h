// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_NODE_H_
#define COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_NODE_H_

#include "base/macros.h"

// Base class representing a node in a DependencyGraph.
class DependencyNode {
 protected:
  // This is intended to be used by the subclasses, not directly.
  DependencyNode() {}
  ~DependencyNode() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DependencyNode);
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_NODE_H_
